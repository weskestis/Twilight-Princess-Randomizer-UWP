"""Exercise the actual container/location hint formatters with all six language entries."""
import argparse, json, os, re, subprocess, tempfile
from pathlib import Path
p = argparse.ArgumentParser()
p.add_argument('source', type=Path)
p.add_argument('--negative-control', action='store_true')
a = p.parse_args()
r = a.source.resolve() / 'mods/randomizer/generator'

def function(text, signature):
    start = text.index(signature); brace = text.index('{', start); depth = 1; pos = brace + 1
    while depth:
        depth += (text[pos] == '{') - (text[pos] == '}'); pos += 1
    return text[start:pos] + '\n'

text_cpp = (r / 'utility/text.cpp').read_text()
hints = (r / 'logic/hints.cpp').read_text()
cpp = r'''
#include "utility/text.hpp"
#include <cassert>
#include <cstdio>
#include <list>
#include <set>
#include <stdexcept>
#include <unordered_set>
namespace randomizer {
namespace utility::str {
bool Contains(const std::string& value, char character) { return value.find(character) != std::string::npos; }
std::string Replace(std::string value, const std::string& old, const std::string& replacement, uint32_t count) {
    size_t position = 0;
    while (count-- && (position = value.find(old, position)) != std::string::npos) {
        value.replace(position, old.size(), replacement); position += replacement.size();
    }
    return value;
}
}
'''
for signature in ['Text::Text(', 'void Text::Replace(const std::string& oldText, const Text&',
    'void Text::Replace(const std::string& oldText, const std::string&',
    'Text& Text::operator+=(const Text&', 'Text& Text::operator+=(const std::string&',
    'Text makeTextListing(']:
    cpp += function(text_cpp, signature)
cpp += r'''
void Text::Capitalize() {}
std::map<std::string, Text> fixtureText;
bool textObjectExists(const std::string& key) { return fixtureText.contains(key); }
const Text& getTextObject(const std::string& key, Text::Type) { return fixtureText.at(key); }
Text getTextObjectForTemplate(const std::string& key, Text::Type type, const std::string&) { return getTextObject(key, type); }
Text addColor(const Text& text, Text::Color, int) { return text; }
namespace logic {
namespace item { struct Item { std::string name; std::string GetName() const { return name; } }; }
namespace area {
struct Area { std::set<std::string> regions; auto GetHintRegions() const { return regions; } };
struct LocationAccess { Area* area; Area* GetArea() const { return area; } };
}
namespace location {
struct Location {
    std::string name; std::unordered_set<std::string> categories;
    std::list<area::LocationAccess*> access; item::Item original{"Nothing"}, reward{"Clawshot"};
    std::string GetName() const { return name; }
    bool HasCategories(const char* category) const { return categories.contains(category); }
    auto GetAccessList() const { return access; }
    item::Item* GetOriginalItem() { return &original; }
    item::Item* GetCurrentItem() { return &reward; }
};
}
namespace hints {
std::string ProcessHintPlurality(const std::string& value, bool) { return value; }
'''
for signature in ['static Text GetGeneratedLocationTextObject(', 'static Text GetLocationTextObject(',
    'static Text GetLocationTextObjectForTemplate(', 'static Text GenerateLocationHintText(']:
    cpp += function(hints, signature)
cpp += '}}}\nint main() {\nusing namespace randomizer; using namespace randomizer::logic;\n'
languages = {'english': 'ENGLISH', 'german': 'GERMAN', 'french': 'FRENCH',
    'spanish': 'SPANISH', 'italian': 'ITALIAN', 'japanese': 'JAPANESE'}
keys = ['Pot Hint Location', 'Pumpkin Hint Location', 'Breakable Hint Region Fallback']
for lang, constant in languages.items():
    data = (r / f'data/text/languages/{lang}.yaml').read_text()
    for key in keys:
        match = re.search(r'^' + re.escape(key) + r':\n  Standard:\n    Text: (.+)$', data, re.M)
        if match:
            value = json.loads(match.group(1))
            assert '<regions>' in value if key != keys[2] else '<regions>' not in value
            cpp += f'fixtureText[{json.dumps(key)}].mEntries[Text::{constant}].str = {json.dumps(value, ensure_ascii=True)};\n'
        elif not a.negative_control:
            raise AssertionError('Missing localized container hint: ' + key + '/' + lang)
cpp += r'''
fixtureText["City in the Sky"] = Text{"the City in the Sky"};
fixtureText["Ordon Village"] = Text{"Ordon Village"};
fixtureText["Clawshot"] = Text{"a Clawshot"};
fixtureText["Location Hint"] = Text{"They say that the reward for <Location Pretty or Cryptic Name> is <Item Pretty or Cryptic Name>."};
area::Area city{{"City in the Sky"}}, ordon{{"Ordon Village"}}, empty{};
area::LocationAccess cityAccess{&city}, secondCityAccess{&city}, ordonAccess{&ordon}, emptyAccess{&empty};
location::Location pot{"Pot 0550 (D_MN07, Room 5)", {"Breakable", "Pot"}, {&cityAccess, &secondCityAccess}};
auto result = hints::GenerateLocationHintText(&pot);
if (result.mEntries[Text::ENGLISH].str.find("0550") != std::string::npos) return 42;
assert(result.mEntries[Text::ENGLISH].str == "They say that the reward for a pot in the City in the Sky is a Clawshot.");
assert(pot.name == "Pot 0550 (D_MN07, Room 5)");
for (const auto lang : supportedLanguages) {
    assert(!result.mEntries[lang].str.empty());
    assert(result.mEntries[lang].str.find("0550") == std::string::npos);
    assert(result.mEntries[lang].str.find("Room") == std::string::npos);
    assert(result.mEntries[lang].str.find("D_MN07") == std::string::npos);
    assert(result.mEntries[lang].str.find("<regions>") == std::string::npos);
}
location::Location pumpkin{"Pumpkin 0007 (F_SP104, Room 2)", {"Breakable", "Pumpkin"}, {&ordonAccess}};
auto label = hints::GetLocationTextObject(&pumpkin, Text::PRETTY);
assert(label.mEntries[Text::ENGLISH].str == "a pumpkin in Ordon Village");
assert(hints::GetLocationTextObjectForTemplate(&pumpkin, Text::CRYPTIC, "Location Hint") == label);
assert(hints::GenerateLocationHintText(&pumpkin).mEntries[Text::ENGLISH].str.find("0007") == std::string::npos);
pot.access = {&cityAccess, &ordonAccess};
label = hints::GetLocationTextObject(&pot, Text::STANDARD);
assert(label.mEntries[Text::ENGLISH].str.find("City in the Sky and Ordon Village") != std::string::npos);
pot.access = {&emptyAccess};
label = hints::GetLocationTextObject(&pot, Text::PRETTY);
assert(label.mEntries[Text::ENGLISH].str == "a pot in Hyrule");
location::Location enemy{"Enemy First Defeat 0001 (F_SP104, Room 2)", {"Enemy First Defeat"}, {&ordonAccess}};
assert(hints::GetLocationTextObject(&enemy, Text::PRETTY).mEntries[Text::ENGLISH].str.find("0001") != std::string::npos);
fixtureText["Known Chest"] = Text{"the known chest"};
location::Location chest{"Known Chest", {}, {&cityAccess}};
assert(hints::GetLocationTextObjectForTemplate(&chest, Text::PRETTY, "Location Hint").mEntries[Text::ENGLISH].str == "the known chest");
puts("PASS: production pot/pumpkin hint formatting in six languages, general regions, no IDs/rooms/stage codes, region deduplication/fallback and exact check identity retained");
}
'''
with tempfile.TemporaryDirectory(prefix='hints-748-') as temp:
    w = Path(temp); source = w / 'test.cpp'; source.write_text(cpp, encoding='utf-8')
    exe = w / ('test.exe' if os.name == 'nt' else 'test')
    command = ['cl', '/nologo', '/std:c++20', '/EHsc', '/utf-8', '/I' + str(r), str(source), '/Fe:' + str(exe)] if os.name == 'nt' else ['c++', '-std=c++20', '-O1', '-I' + str(r), str(source), '-o', str(exe)]
    subprocess.run(command, cwd=w, check=True)
    result = subprocess.run([str(exe)], cwd=w)
    if a.negative_control:
        assert result.returncode == 42, 'Shipped .747 did not reproduce the internal container-number leak'
        print('PASS: shipped .747 leaks the internal pot number into the player hint')
    else:
        result.check_returncode()
