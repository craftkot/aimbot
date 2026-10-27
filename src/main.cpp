#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
using namespace geode::prelude;

namespace {
enum class Style { Neon, Tech, Ruins, Nature };
struct Cell { int x, y; bool operator==(Cell const& other) const { return x == other.x && y == other.y; } };
struct CellHash {
    size_t operator()(Cell const& c) const noexcept {
        return (static_cast<uint64_t>(static_cast<uint32_t>(c.x)) << 32) ^
               static_cast<uint32_t>(c.y);
    }
};
uint32_t hash(int x, int y, int salt) {
    uint32_t n = 2166136261u;
    for (uint32_t v : {static_cast<uint32_t>(x), static_cast<uint32_t>(y), static_cast<uint32_t>(salt)}) {
        n = (n ^ v) * 16777619u;
    }
    n ^= n >> 16; n *= 0x7feb352du; n ^= n >> 15;
    return n;
}
float random01(int x, int y, int seed) { return (hash(x, y, seed) % 10000) / 10000.f; }
std::vector<int> candidateIDs(Style style, bool edge) {
    switch (style) {
        case Style::Neon: return edge ? std::vector<int>{227,228,236,279,280} : std::vector<int>{279,280,281,282};
        case Style::Tech: return edge ? std::vector<int>{237,238,239,240,241} : std::vector<int>{231,232,283,284,285};
        case Style::Ruins: return edge ? std::vector<int>{233,234,235,245,246} : std::vector<int>{277,278,279};
        case Style::Nature: return edge ? std::vector<int>{222,223,224,406,407} : std::vector<int>{461,462,463,464,224};
    }
    return {};
}
std::vector<int> safeIDs(std::vector<int> const& ids) {
    std::vector<int> result;
    for (auto id : ids) {
        auto* obj = GameObject::createWithKey(id);
        if (obj && obj->m_objectType == GameObjectType::Decoration) result.push_back(id);
    }
    return result;
}
struct Piece { int id; CCPoint pos; float scale; float rotation; };
}

class $modify(AutoDecorEditor, EditorUI) {
    struct Fields {
        CCMenu* panel = nullptr;
        std::vector<int> lastIDs;
    };

    CCMenuItemSpriteExtra* makeButton(char const* name, SEL_MenuHandler cb, float scale=0.42f) {
        auto* label = CCLabelBMFont::create(name, "goldFont.fnt");
        label->setScale(scale);
        return CCMenuItemSpriteExtra::create(label, this, cb);
    }
    bool init(LevelEditorLayer* layer) {
        if (!EditorUI::init(layer)) return false;
        auto win = CCDirector::sharedDirector()->getWinSize();
        auto* open = CCMenu::create();
        open->setPosition({65.f, win.height - 54.f});
        open->addChild(makeButton("AUTO DECO", menu_selector(AutoDecorEditor::togglePanel), 0.40f));
        open->setID("auto-decor-trigger");
        this->addChild(open, 500);
        auto* panel = CCMenu::create();
        panel->setPosition({80.f, win.height - 115.f});
        panel->addChild(makeButton("NEON", menu_selector(AutoDecorEditor::neon)));
        panel->addChild(makeButton("TECH", menu_selector(AutoDecorEditor::tech)));
        panel->addChild(makeButton("RUINS", menu_selector(AutoDecorEditor::ruins)));
        panel->addChild(makeButton("NATURE", menu_selector(AutoDecorEditor::nature)));
        panel->addChild(makeButton("REVERT", menu_selector(AutoDecorEditor::revert), 0.34f));
        panel->alignItemsVerticallyWithPadding(6.f);
        panel->setVisible(false);
        panel->setID("auto-decor-style-panel");
        this->addChild(panel, 501);
        m_fields->panel = panel;
        return true;
    }
    void note(std::string const& message) {
        FLAlertLayer::create("Auto Decor", message.c_str(), "OK")->show();
    }
    void togglePanel(CCObject*) {
        auto* panel = m_fields->panel;
        if (panel) panel->setVisible(!panel->isVisible());
    }
    void neon(CCObject*) { decorate(Style::Neon); }
    void tech(CCObject*) { decorate(Style::Tech); }
    void ruins(CCObject*) { decorate(Style::Ruins); }
    void nature(CCObject*) { decorate(Style::Nature); }
    void decorate(Style style) {
        if (m_fields->panel) m_fields->panel->setVisible(false);
        auto* layer = this->m_editorLayer;
        auto* selected = getSelectedObjects();
        if (!layer || !selected || selected->count() == 0) {
            note("Select some regular square blocks first.");
            return;
        }
        if (selected->count() > 700) {
            note("Select 700 objects or fewer.");
            return;
        }
        std::unordered_map<Cell, CCPoint, CellHash> cells;
        for (unsigned i = 0; i < selected->count(); ++i) {
            auto* obj = static_cast<GameObject*>(selected->objectAtIndex(i));
            if (!obj || obj->m_objectType != GameObjectType::Solid) continue;
            if (std::abs(obj->getRotation()) > .1f || std::abs(obj->getScaleX() - 1.f) > .04f ||
                std::abs(obj->getScaleY() - 1.f) > .04f) continue;
            auto p = obj->getPosition();
            if (!std::isfinite(p.x) || !std::isfinite(p.y)) continue;
            Cell key{static_cast<int>(std::lround(p.x/30.f)), static_cast<int>(std::lround(p.y/30.f))};
            cells[key] = p;
        }
        if (cells.empty()) {
            note("No regular, unrotated full blocks in selection.");
            return;
        }
        auto edges = safeIDs(candidateIDs(style, true));
        auto details = safeIDs(candidateIDs(style, false));
        if (edges.empty() && details.empty()) {
            note("This style has no supported decoration IDs on this GD version. No changes made.");
            return;
        }
        std::vector<Cell> ordered;
        for (auto const& [cell, pos] : cells) ordered.push_back(cell);
        std::sort(ordered.begin(), ordered.end(), [](Cell const& a, Cell const& b) {
            return a.x == b.x ? a.y < b.y : a.x < b.x;
        });
        std::vector<Piece> pieces;
        const auto occupied = [&cells](Cell c) { return cells.contains(c); };
        for (auto cell : ordered) {
            if (pieces.size() >= 220) break;
            auto p = cells.at(cell);
            bool top = !occupied({cell.x,cell.y+1});
            bool bottom = !occupied({cell.x,cell.y-1});
            bool left = !occupied({cell.x-1,cell.y});
            bool right = !occupied({cell.x+1,cell.y});
            auto rnd = hash(cell.x, cell.y, 11);
            if ((top||bottom||left||right) && !edges.empty() && random01(cell.x,cell.y,6) < .75f) {
                float dx = 0, dy = 0, angle = 0;
                if (top) dy = 9;
                else if (bottom) { dy = -9; angle = 180; }
                else if (left) { dx = -9; angle = 90; }
                else if (right) { dx = 9; angle = -90; }
                pieces.push_back({edges[rnd % edges.size()], {p.x+dx,p.y+dy}, .48f, angle});
            }
            if (pieces.size() < 220 && !details.empty() && random01(cell.x,cell.y,12) < .22f) {
                pieces.push_back({details[(rnd>>7) % details.size()],
                    {p.x+static_cast<float>((rnd>>12)%9)-4, p.y}, .28f, float((rnd>>22)%4)*90});
            }
        }
        if (pieces.empty()) {
            note("No decorations generated. Try a larger selection.");
            return;
        }
        std::vector<int> ids;
        size_t count = 0;
        for (auto const& p : pieces) {
            auto* o = layer->createObject(p.id, p.pos, false);
            if (!o) continue;
            o->setScale(p.scale);
            o->setRotation(p.rotation);
            o->m_zLayer = ZLayer::B2;
            o->m_isNoTouch = true;
            if (o->m_uniqueID > 0) ids.push_back(o->m_uniqueID);
            ++count;
        }
        m_fields->lastIDs = std::move(ids);
        note("Placed " + std::to_string(count) + " decorations. Test your level before saving.");
    }
    void revert(CCObject*) {
        if (m_fields->panel) m_fields->panel->setVisible(false);
        auto* layer = this->m_editorLayer;
        if (!layer || m_fields->lastIDs.empty()) {
            note("Nothing to revert; use native Undo if needed.");
            return;
        }
        size_t n = 0;
        for (auto uid : m_fields->lastIDs) {
            if (auto* o = layer->findGameObject(uid)) {
                layer->removeObject(o, false);
                ++n;
            }
        }
        m_fields->lastIDs.clear();
        note("Removed " + std::to_string(n) + " decoration objects.");
    }
};
