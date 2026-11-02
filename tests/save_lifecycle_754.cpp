#include "legacy-vanilla-fixture.inc"

using namespace std::chrono_literals;
std::function<void(mDoMemCd_Ctrl_c&)> workerStore;
void mDoMemCd_Ctrl_c::restore() { field_0x1fc8=1; }
void mDoMemCd_Ctrl_c::store() { if(workerStore)workerStore(*this); }
void mDoMemCd_Ctrl_c::format() { field_0x1fc8=1; }
void mDoMemCd_Ctrl_c::attach() { mCardState=CARD_STATE_READY_e;field_0x1fc8=1; }
void mDoMemCd_Ctrl_c::detach() { mCardState=CARD_STATE_NO_CARD_e;field_0x1fc8=1; }

// Force another real thread to own the mutex immediately after the caller's
// first unlock. In .749 that is the filename lookup, before its dropping try-lock.
void contend_after_first_unlock(mDoMemCd_Ctrl_c& card, const std::function<void()>& action) {
    std::promise<OSMutex*> acquire;
    std::promise<void> acquired, release;
    auto ready=acquired.get_future();auto done=release.get_future();
    std::thread holder([&] {
        auto* mutex=acquire.get_future().get();
        OSLockMutex(mutex);acquired.set_value();done.wait();OSUnlockMutex(mutex);
    });
    afterUnlock=[&](OSMutex* mutex) { acquire.set_value(mutex);ready.wait(); };
    action();
    release.set_value();holder.join();
}

int main(int argc,char**argv) {
    try {
        require(argc==2,"data directory required");
        const auto root=std::filesystem::absolute(argv[1]);
        auto legacyDir=(root/"legacy").string();char* legacyArgs[]={argv[0],legacyDir.data()};
        require(legacy_main(2,legacyArgs)==0,"Existing GCI/save/import/isolation coverage failed");
        fprintf(stderr,"Verified existing save cases\n");
        dusk::ConfigPath=root/"threaded";
        std::filesystem::create_directories(dusk::ConfigPath/"USA/Card A");
        std::array<u8,SAVEFILE_SIZE> quest{};
        for(size_t i=0;i<quest.size();++i)quest[i]=u8(i*23);
        for(u8 i=0;i<3;++i)mDoMemCdRWm_SetCheckSumGameData(quest.data(),i);
        std::vector<u8> packet;mDoMemCdRWm_BuildXboxGci(quest.data(),packet);
        write(xboxVanillaSavePath(),packet);
        mDoMemCd_Ctrl_c card;card.ThdInit();
        unsigned dropped=0;
        contend_after_first_unlock(card,[&]{card.load();});
        if(card.mCardState!=CARD_STATE_READ_e)++dropped;
        card.setFileName("gczelda2");
        contend_after_first_unlock(card,[&]{card.save(quest.data(),quest.size(),0);});
        if(card.mCardState!=CARD_STATE_WRITE_e)++dropped;
        card.mCardState=CARD_STATE_NO_CARD_e;
        contend_after_first_unlock(card,[&]{card.command_attach();});
        if(card.mCardState!=CARD_STATE_READY_e)++dropped;
        fprintf(stderr,"Verified contended load/save/attach\n");

        card.setFileName("randomizer-xbox");
        card.save(quest.data(),quest.size(),0);
        const char* snapshot=card.getFileName();
        std::thread changeName([&]{card.setFileName("gczelda2");});changeName.join();
        const bool danglingSnapshot=std::string(snapshot)!="randomizer-xbox";
        fprintf(stderr,"Verified filename lifetime\n");

        std::promise<void> storeEntered,finishStore;
        auto canFinish=finishStore.get_future();
        std::string beforeName,afterName;
        workerStore=[&](mDoMemCd_Ctrl_c& controller){
            beforeName=controller.getFileName();storeEntered.set_value();canFinish.wait();
            afterName=controller.getFileName();controller.field_0x1fc8=1;
        };
        card.mCardCommand=COMM_STORE_e;
        std::thread worker([&]{card.main();});storeEntered.get_future().wait();
        auto switchMode=std::async(std::launch::async,[&]{card.setFileName("randomizer-xbox");});
        const bool blockedSwitch=switchMode.wait_for(200ms)!=std::future_status::ready;
        if(!blockedSwitch)require(card.getStatus(0)==14,"Pending mode switch exposed old save status");
        finishStore.set_value();switchMode.get();
        fprintf(stderr,"Released old worker operation\n");
        card.load();
        require(beforeName=="gczelda2"&&afterName=="gczelda2","In-flight write changed its target file");
        require(card.mCardState==CARD_STATE_READ_e,"Deferred mode failed to refresh and load");
        OSLockMutex(&card.mMutex);dusk::IsShuttingDown=true;OSUnlockMutex(&card.mMutex);
        OSSignalCond(&card.mCond);worker.join();dusk::IsShuttingDown=false;

        for(unsigned i=0;i<250;++i) {
            card.setFileName(i%2?"gczelda2":"randomizer-xbox");
            card.load();std::array<u8,SAVEFILE_SIZE> read{};
            require(card.LoadSync(read.data(),read.size(),0)==1&&read==quest,"Mode-cycle save mismatch");
            card.save(read.data(),read.size(),0);
            require(card.SaveSync()==1,"Mode-cycle save did not complete");
        }
#ifndef EXPECT_749_FAILURES
        card.setFileName("randomizer-xbox");
        const auto randoPath=xboxRandomizerSavePath();
        const auto before=bytes(randoPath);
        auto changed=quest;changed[4]^=0x55;mDoMemCdRWm_SetCheckSumGameData(changed.data(),0);
        card.save(changed.data(),changed.size(),0);
        require(bytes(randoPath.string()+".bak")==before,"Randomizer did not retain its preceding complete save");
        std::filesystem::remove(randoPath); // interrupted replacement, in the temporary test directory
        card.command_attach();card.load();
        require(card.mCardState==CARD_STATE_READ_e&&bytes(randoPath)==before,"Randomizer backup recovery failed");
        auto corrupt=before;corrupt[0]^=0x55;write(randoPath,corrupt);
        card.command_attach();
        require(card.mCardState==CARD_STATE_FATAL_ERROR_e&&bytes(randoPath)==corrupt,"Corrupt Randomizer save was silently replaced");
        write(randoPath,before);std::filesystem::create_directory(randoPath.string()+".tmp");
        card.save(changed.data(),changed.size(),0);
        require(card.mCardState==CARD_STATE_FATAL_ERROR_e&&bytes(randoPath)==before,"Failed Randomizer write damaged the original");
#endif
#ifdef EXPECT_749_FAILURES
        require(dropped==3&&blockedSwitch&&danglingSnapshot,"Baseline did not reproduce all known concurrency defects");
        puts("PASS negative control: .749 drops 3 contended requests, blocks mode handoff, invalidates filename snapshot");
#else
        require(dropped==0&&!blockedSwitch&&!danglingSnapshot,"Save lifecycle regression");
        puts("PASS: real mutex contention, deferred worker handoff, stable filenames, 250 save/load mode cycles and legacy GCI coverage");
#endif
        return 0;
    } catch(const std::exception& e) { fprintf(stderr,"%s\n",e.what());return 1; }
}
