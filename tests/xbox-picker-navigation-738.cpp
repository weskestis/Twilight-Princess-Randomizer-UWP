#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#include "ui.hpp"
#include "document.hpp"
#include "ui/xbox_file_picker.cpp"

std::string testBasePath;
namespace dusk::data { std::filesystem::path configured; }
namespace dusk::ui {
Rml::Context* testContext = nullptr;
std::vector<std::unique_ptr<Document>> documents;
Document* top_document() noexcept {
    for (auto it = documents.rbegin(); it != documents.rend(); ++it) {
        if ((*it)->active()) return it->get();
    }
    return nullptr;
}
void push_document(std::unique_ptr<Document> document, bool show, bool) noexcept {
    auto* ptr = document.get();
    documents.push_back(std::move(document));
    if (show) ptr->show();
}
void uncover_top_document() noexcept { if (auto* doc = top_document()) doc->uncover(); }
void apply_scoped_styles(Document& document) noexcept { document.restyle({}); }
bool game_obscured_below(const Document&) noexcept { return false; }
}
namespace borealis::file_select {
void install_platform_backend(PlatformBackend) noexcept {}
void pump_completions() {}
namespace detail {
Result copy_export_file(std::string_view source, std::string_view destination, bool) {
    std::filesystem::copy_file(dusk::ui::xbox_file_picker::model::path(source),
        dusk::ui::xbox_file_picker::model::path(destination), std::filesystem::copy_options::overwrite_existing);
    return {.status=Status::Selected, .locations={std::string(destination)}};
}
}
}
class System final : public Rml::SystemInterface {
public:
    double elapsed = 1;
    double GetElapsedTime() override { return elapsed; }
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override {
        if (type <= Rml::Log::LT_ERROR) std::cerr << message << '\n';
        return true;
    }
};
class Renderer final : public Rml::RenderInterface {
public:
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override { return 1; }
    void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override {}
    void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
    Rml::TextureHandle LoadTexture(Rml::Vector2i&, const Rml::String&) override { return 0; }
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>, Rml::Vector2i) override { return 1; }
    void ReleaseTexture(Rml::TextureHandle) override {}
    void EnableScissorRegion(bool) override {}
    void SetScissorRegion(Rml::Rectanglei) override {}
};
namespace ui = dusk::ui;
namespace pick = dusk::ui::xbox_file_picker;
namespace fileSelect = borealis::file_select;
System systemInterface;
void tick() {
    systemInterface.elapsed += 0.04;
    pick::update();
    for (size_t i = 0; i < ui::documents.size(); ++i) ui::documents[i]->update();
    ui::testContext->Update();
    std::erase_if(ui::documents, [](const auto& doc) { return doc->closed(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}
template<class Predicate> void until(Predicate ready) {
    for (int i = 0; i < 1500; ++i) { tick(); if (ready()) return; }
    throw std::runtime_error("Picker did not reach the expected UI state");
}
Rml::ElementDocument* document() {
    auto* focused = ui::testContext->GetFocusElement();
    return focused ? focused->GetOwnerDocument() : ui::testContext->GetDocument(0);
}
Rml::String text(Rml::Element* element) {
    if (auto* node = dynamic_cast<Rml::ElementText*>(element)) return node->GetText();
    Rml::String result;
    for (int i = 0; element && i < element->GetNumChildren(); ++i) result += text(element->GetChild(i));
    return result;
}
void key(Rml::Input::KeyIdentifier code) {
    ui::testContext->ProcessKeyDown(code, 0);
    ui::testContext->ProcessKeyUp(code, 0);
    for (int i = 0; i < 4; ++i) tick();
}
void press(const Rml::String& label) {
    Rml::ElementList buttons;
    document()->QuerySelectorAll(buttons, "button");
    for (auto* button : buttons) if (text(button) == label) {
        button->RemoveProperty("visibility");
        assert(button->Focus(true));
        key(Rml::Input::KI_RETURN);
        return;
    }
    throw std::runtime_error("Missing picker action: " + label);
}
void loaded(const std::string& location) {
    until([&] {
        return document() && pick::g_request &&
            pick::model::same_location(text(document()->QuerySelector("picker-path")), location) &&
            text(document()->QuerySelector("modal-body")).find("Reading folders...") == std::string::npos;
    });
    for (int i = 0; i < 8; ++i) tick();
}
void check_layout(int width, int height) {
    auto* doc = document();
    auto* window = doc->GetElementById("window");
    auto* viewport = doc->QuerySelector("ui-list-viewport");
    std::cout << "Viewport " << width << 'x' << height << ": window "
        << window->GetOffsetWidth() << 'x' << window->GetOffsetHeight()
        << ", list " << viewport->GetClientWidth() << 'x' << viewport->GetClientHeight() << '\n';
    assert(window->GetOffsetWidth() > width * 0.75f);
    assert(window->GetOffsetHeight() > height * 0.70f);
    assert(viewport->GetClientHeight() > height * 0.40f);
    Rml::ElementList buttons;
    doc->QuerySelectorAll(buttons, "button.modal-btn");
    assert(buttons.size() == 2);
    const auto origin = window->GetAbsoluteOffset(Rml::BoxArea::Border);
    for (auto* button : buttons) {
        const auto position = button->GetAbsoluteOffset(Rml::BoxArea::Border);
        assert(position.x >= origin.x && position.y >= origin.y);
        assert(position.x + button->GetOffsetWidth() <= origin.x + window->GetOffsetWidth() + 1);
        assert(position.y + button->GetOffsetHeight() <= origin.y + window->GetOffsetHeight() + 1);
        assert(button->GetOffsetWidth() > 200);
    }
    assert(doc->QuerySelector("picker-path")->GetOffsetHeight() < height * 0.15f);
}
int main() {
    Renderer renderer;
    Rml::SetSystemInterface(&systemInterface);
    Rml::SetRenderInterface(&renderer);
    assert(Rml::Initialise());
    for (const auto& family : {"Fira Sans", "Fira Sans Condensed"}) {
        assert(Rml::LoadFontFace("font.ttf", family, Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal));
        assert(Rml::LoadFontFace("font.ttf", family, Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Bold));
    }
    ui::testContext = Rml::CreateContext("picker", {1280, 720});
    const auto base = std::filesystem::current_path()/"filesystem-fixture";
    const auto development = base/"DevelopmentFiles";
    const auto install = development/"WindowsApps"/"TwilightPrincessRandomizer";
    const auto data = base/"LocalState";
    std::filesystem::create_directories(install/"Games");
    std::filesystem::create_directories(data);
    std::filesystem::create_directories(base/"LocalCache");
    dusk::data::configured = data;
    testBasePath = pick::model::utf8(install);
    auto touch = [](const std::filesystem::path& path) { std::ofstream(path, std::ios::binary) << "fixture"; };
    touch(install/"Games"/"Twilight Princess.RVZ");
    touch(install/"Games"/"ignore.txt");
    for (int i = 0; i < 405; ++i) touch(install/("image-"+std::to_string(i)+".iso"));
    touch(install/"filename-with-long-text-and-<markup>-&-unicode-\xc3\xa9.iso");
    int completed = 0;
    fileSelect::Result result;
    auto receive = [&](fileSelect::Result value) { ++completed; result = std::move(value); };
    pick::open_file({.filters={{"Game Disc Images", "iso;gcm;rvz"}, {"All Files", "*"}}}, receive);
    assert(pick::g_request->shortcuts.front().location == "D:\\DevelopmentFiles");
    assert(pick::g_request->shortcuts.front().label == "Development Files");
    // Substitute the fixture's physical mount at the external filesystem boundary.
    // The Browser, filesystem scanner, list, focus and event handlers remain production code.
    pick::g_request->shortcuts.front().location = pick::model::utf8(development);
    loaded("Choose a location");
    assert(text(ui::testContext->GetFocusElement()) == "Development Files");
    for (const auto dimensions : {Rml::Vector2i{1280, 720}, {1920, 1080}, {3840, 2160}}) {
        ui::testContext->SetDimensions(dimensions);
        ui::testContext->SetDensityIndependentPixelRatio(dimensions.x == 3840 ? 2.f : 1.f);
        for (int i = 0; i < 6; ++i) tick();
        check_layout(dimensions.x, dimensions.y);
    }
    ui::testContext->SetDimensions({1280,720});
    ui::testContext->SetDensityIndependentPixelRatio(1);
    key(Rml::Input::KI_DOWN);
    assert(text(ui::testContext->GetFocusElement()) == "App Folder (installed game files)");
    key(Rml::Input::KI_RETURN);
    loaded(pick::model::utf8(install));
    assert(text(ui::testContext->GetFocusElement()) == "[Folder] Games");
    key(Rml::Input::KI_NEXT);
    until([&] { return text(document()->QuerySelector("modal-body")).find("Page 2 of") != std::string::npos; });
    key(Rml::Input::KI_PRIOR);
    until([&] { return text(document()->QuerySelector("modal-body")).find("Page 1 of") != std::string::npos; });
    press("[Folder] Games");
    loaded(pick::model::utf8(install/"Games"));
    assert(text(ui::testContext->GetFocusElement()) == "Twilight Princess.RVZ");
    key(Rml::Input::KI_ESCAPE);
    loaded(pick::model::utf8(install));
    assert(text(ui::testContext->GetFocusElement()) == "[Folder] Games");
    press("[Folder] Games");
    loaded(pick::model::utf8(install/"Games"));
    press("File Type: Game Disc Images");
    loaded(pick::model::utf8(install/"Games"));
    assert(text(document()->QuerySelector("ui-list-content")).find("ignore.txt") != std::string::npos);
    press("Twilight Princess.RVZ");
    until([&] { return completed == 1; });
    assert(result.status == fileSelect::Status::Selected && result.locations.front() == pick::model::utf8(install/"Games"/"Twilight Princess.RVZ"));
    for (int i=0;i<10;++i) tick();
    assert(completed == 1 && ui::documents.empty());

    pick::open_folder({.defaultLocation=pick::model::utf8(data)}, receive);
    loaded(pick::model::utf8(data));
    assert(text(ui::testContext->GetFocusElement()) == "Choose This Folder");
    key(Rml::Input::KI_ESCAPE);
    loaded("Choose a location");
    assert(completed == 1);  // Back does not choose a folder or close prematurely.
    press("App Data (saves, seeds and mods)");
    loaded(pick::model::utf8(data));
    press("Choose This Folder");
    until([&] { return completed == 2; });
    assert(result.status == fileSelect::Status::Selected && result.locations.front() == pick::model::utf8(data));

    pick::open_file({}, receive);
    pick::g_request->shortcuts.front().location = pick::model::utf8(base/"missing-DevelopmentFiles");
    loaded("Choose a location");
    press("Development Files");
    loaded(pick::model::utf8(base/"missing-DevelopmentFiles"));
    assert(text(document()->QuerySelector("modal-body")).find("Cannot read") != std::string::npos);
    key(Rml::Input::KI_ESCAPE);
    loaded("Choose a location");
    key(Rml::Input::KI_ESCAPE);
    until([&] { return completed == 3; });
    assert(result.status == fileSelect::Status::Canceled && dusk::data::configured == data);
    assert(ui::documents.empty());

    const auto exportSource = base/"source.gci";
    touch(exportSource);
    pick::export_file({.sourceLocation=pick::model::utf8(exportSource), .suggestedName="save.gci"}, receive);
    loaded(pick::model::utf8(data));
    press("Export Here: save.gci");
    until([&] { return completed == 4; });
    assert(result.status == fileSelect::Status::Selected && std::filesystem::exists(data/"save.gci"));
    pick::export_file({.sourceLocation=pick::model::utf8(exportSource), .suggestedName="save.gci"}, receive);
    loaded(pick::model::utf8(data));
    press("Export Here: save.gci");
    until([&] { return document() && text(document()->QuerySelector("modal-title")) == "Replace Existing File?"; });
    key(Rml::Input::KI_ESCAPE);
    until([&] { return !pick::g_request->confirming && ui::top_document() && ui::top_document()->active(); });
    assert(completed == 4 && std::filesystem::file_size(data/"save.gci") == 7);
    press("Cancel");
    until([&] { return completed == 5; });
    assert(result.status == fileSelect::Status::Canceled && ui::documents.empty());
    Rml::RemoveContext("picker");
    ui::testContext = nullptr;
    Rml::Shutdown();
    std::cout << "PASS: production RmlUi picker layout at 720p/1080p/4K, named mounts, controller focus, open/back/reselection, paging, filters, selection, missing folders, cancel and overwrite/export UI\n";
}
