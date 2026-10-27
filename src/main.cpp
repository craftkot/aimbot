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
enum class Detail { Clean, Balanced, Rich };
enum class Scope { Selected, WholeLevel };
struct InkPalette { std::array<ccColor3B, 3> colors; };
InkPalette inkPalette(Style s) {
    switch(s) {
        case Style::Neon: return {{{18,30,64},{35,205,250},{151,70,255}}};
        case Style::Tech: return {{{45,64,83},{89,195,236},{255,193,79}}};
        case Style::Ruins: return {{{102,68,62},{190,119,91},{238,189,121}}};
        case Style::Nature: return {{{50,87,60},{110,190,92},{212,191,106}}};
    }
    return {{{70,70,70},{190,190,190},{255,255,255}}};
}


struct Cell {
    int x, y;
    bool operator==(Cell const& other) const { return x == other.x && y == other.y; }
};
struct CellHash {
    size_t operator()(Cell const& c) const noexcept {
        return (static_cast<uint64_t>(static_cast<uint32_t>(c.x)) << 32)
              ^ static_cast<uint32_t>(c.y);
    }
};

uint32_t scramble(int x, int y, int salt) {
    uint32_t h = 0x811c9dc5u;
    for (auto n : {x, y, salt}) {
        h ^= static_cast<uint32_t>(n);
        h *= 0x01000193u;
    }
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16;
    return h;
}
float probability(int x, int y, int salt) {
    return (scramble(x, y, salt) % 10000) / 10000.f;
}
int count(bool a, bool b, bool c, bool d) {
    return int(a)+int(b)+int(c)+int(d);
}
int posmod(int n, int d) { return (n % d + d) % d; }

// Every ID below is a catalogued Geometry Dash editor object:
// Neon 1162..1196, Mechanic 263..268, Brick 269..278, Grass 952..959.
// Object families are internally consistent instead of random decor sprites.
struct Palette {
    int face, top, outer, inner, square, pillar, pillarTop, horizontal, left;
};
Palette palette(Style s) {
    switch (s) {
        case Style::Neon: return {1166,1162,1163,1168,1170,1165,1164,1174,1167};
        case Style::Tech: return {263,264,265,266,263,268,267,268,264};
        case Style::Ruins:return {269,270,271,272,269,275,274,275,270};
        case Style::Nature:return {955,952,953,954,959,957,956,957,952};
    }
    return {1166,1162,1163,1168,1170,1165,1164,1174,1167};
}
char const* name(Style s) {
    switch (s) {
        case Style::Neon: return "NEON";
        case Style::Tech: return "TECH";
        case Style::Ruins: return "RUINS";
        case Style::Nature: return "NATURE";
    }
    return "DECOR";
}
char const* densityName(Detail d) {
    return d == Detail::Clean ? "CLEAN" : d == Detail::Balanced ? "NORMAL" : "RICH";
}
struct Neighborhood {
    bool t, r, b, l; // exposed boundaries (no block on that side)
    bool tl, tr, br, bl; // absent diagonal blocks
    int exposed() const { return count(t,r,b,l); }
};
struct Tile {
    int id;
    float angle;
};
Tile chooseTile(Palette p, Neighborhood n, Style s) {
    int k = n.exposed();
    if (k >= 3) return {p.square, 0};
    if (k == 2) {
        if (n.t && n.b) return {p.pillar, 0}; // vertical ribbon
        if (n.r && n.l)
            return {p.horizontal, s == Style::Neon ? 0.f : 90.f};
        if (n.t && n.l) return {p.outer, 0};
        if (n.t && n.r) return {p.outer, 90};
        if (n.b && n.r) return {p.outer, 180};
        return {p.outer, 270};
    }
    if (k == 1) {
        if (n.t) return {p.top, 0};
        if (n.r) return {p.top, 90};
        if (n.b) return {p.top, 180};
        return {p.left, s == Style::Neon ? 0.f : 270.f};
    }
    // If all adjacent blocks exist but diagonal is absent, this is an
    // interior notch; use a real inside corner instead of a central fill.
    if (n.tl) return {p.inner, 0};
    if (n.tr) return {p.inner, 90};
    if (n.br) return {p.inner, 180};
    if (n.bl) return {p.inner, 270};
    return {p.face, 0};
}
struct Piece {
    int id;
    CCPoint pos;
    float scale = 1.f;
    float angle = 0.f;
    ZLayer layer = ZLayer::T1;
    int zOrder = 0;
    int ink = 0; // 0 structural, 1 trim, 2 highlight
};
constexpr size_t kMaxBlocks = 20000;
constexpr size_t kMaxPieces = 24000;
constexpr size_t kFrameBudget = 85;
}

class $modify(AutoDecorEditor, EditorUI) {
    struct Fields {
        CCMenu* panel = nullptr;
        Detail detail = Detail::Balanced;
        Scope scope = Scope::WholeLevel;
        std::vector<std::vector<int>> history;
        std::vector<Piece> work;
        std::array<std::array<int,3>,4> colorChannels{};
        std::array<ccColor3B,3> activeRGB{};
        std::array<int,3> activeColorIDs{};
        size_t anchorCount=0;
        std::vector<int> liveIDs;
        size_t cursor = 0, created = 0, rejected = 0, inspected = 0, eligible = 0;
        bool busy = false;
        Style activeStyle = Style::Neon;
    };

    CCMenuItemSpriteExtra* button(char const* label, SEL_MenuHandler cb, float scale=0.39f) {
        auto* text = CCLabelBMFont::create(label, "goldFont.fnt");
        text->setScale(scale);
        return CCMenuItemSpriteExtra::create(text, this, cb);
    }
    bool init(LevelEditorLayer* layer) {
        if (!EditorUI::init(layer)) return false;
        auto size = CCDirector::sharedDirector()->getWinSize();
        auto* opener = CCMenu::create();
        opener->setPosition({70.f, size.height - 50.f});
        opener->addChild(button("AUTO DECO", menu_selector(AutoDecorEditor::togglePanel)));
        opener->setID("auto-decor-trigger");
        this->addChild(opener, 500);

        auto* panel = CCMenu::create();
        panel->setPosition({90.f, size.height - 130.f});
        panel->addChild(button("NEON", menu_selector(AutoDecorEditor::neon)));
        panel->addChild(button("TECH", menu_selector(AutoDecorEditor::tech)));
        panel->addChild(button("RUINS", menu_selector(AutoDecorEditor::ruins)));
        panel->addChild(button("NATURE", menu_selector(AutoDecorEditor::nature)));
        panel->addChild(button("TARGET: ALL BLOCKS", menu_selector(AutoDecorEditor::scopeAll), 0.28f));
        panel->addChild(button("TARGET: SELECTED", menu_selector(AutoDecorEditor::scopeSelected), 0.28f));
        panel->addChild(button("DETAIL: CLEAN", menu_selector(AutoDecorEditor::clean), 0.32f));
        panel->addChild(button("DETAIL: NORMAL", menu_selector(AutoDecorEditor::normal), 0.32f));
        panel->addChild(button("DETAIL: RICH", menu_selector(AutoDecorEditor::rich), 0.32f));
        panel->addChild(button("UNDO DECO", menu_selector(AutoDecorEditor::revert), 0.33f));
        panel->alignItemsVerticallyWithPadding(5.f);
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
        auto* p = m_fields->panel;
        if (p) p->setVisible(!p->isVisible());
    }
    void clean(CCObject*) { m_fields->detail = Detail::Clean; note("Detail set to CLEAN. Pick a theme."); }
    void normal(CCObject*) { m_fields->detail = Detail::Balanced; note("Detail set to NORMAL. Pick a theme."); }
    void rich(CCObject*) { m_fields->detail = Detail::Rich; note("Detail set to RICH. Pick a theme."); }
    void scopeAll(CCObject*) {
        m_fields->scope = Scope::WholeLevel;
        note("Target: ALL solid blocks of the level. Select a theme.");
    }
    void scopeSelected(CCObject*) {
        m_fields->scope = Scope::Selected;
        note("Target: selected objects only. Select a theme.");
    }
    void neon(CCObject*) { decorate(Style::Neon); }
    void tech(CCObject*) { decorate(Style::Tech); }
    void ruins(CCObject*) { decorate(Style::Ruins); }
    void nature(CCObject*) { decorate(Style::Nature); }

    void decorate(Style style) {
        if (m_fields->busy) {
            note("Decoration is still generating. Try again when finished.");
            return;
        }
        if (m_fields->panel) m_fields->panel->setVisible(false);
        auto* layer = this->m_editorLayer;
        if (!layer) { note("Editor is not active."); return; }
        auto* input = m_fields->scope == Scope::WholeLevel
            ? layer->getAllObjects() : this->getSelectedObjects();
        if (!input || input->count() == 0) {
            note("No objects found. Add blocks or switch TARGET mode.");
            return;
        }
        // Do not mutate the live array while reading it: object creation below
        // occurs only after selection/level contents have been snapshotted.
        // Pass A: classify *all* existing level objects once. A full solid
        // tile is different from slopes, portals and decorations: those
        // receive only background supports, never opaque tile overlays.
        struct Candidate { CCPoint pos; int id; };
        struct Anchor { CCPoint pos; int type; };
        std::vector<Candidate> terrain;
        std::vector<Anchor> anchors;
        terrain.reserve(std::min<size_t>(input->count(), kMaxBlocks));
        anchors.reserve(std::min<size_t>(input->count(), kMaxBlocks));
        std::array<size_t,900> lattice{};
        size_t inspected = 0, filtered = 0, slopes = 0;
        for (unsigned i=0; i<input->count() && inspected<100000; ++i) {
            auto* o=static_cast<GameObject*>(input->objectAtIndex(i));
            ++inspected;
            if (!o) continue;
            auto pt=o->getPosition();
            if (!std::isfinite(pt.x)||!std::isfinite(pt.y)) {++filtered;continue;}
            // Existing non-block elements become background composition
            // anchors; gameplay triggers remain untouched.
            auto type=o->m_objectType;
            bool terrainType=(type==GameObjectType::Solid || type==GameObjectType::Breakable);
            bool isSlope=type==GameObjectType::Slope;
            bool isGameplayAnchor=(isSlope || type==GameObjectType::Decoration ||
                type==GameObjectType::Hazard || type==GameObjectType::AnimatedHazard ||
                type==GameObjectType::Special);
            // Skip decoration produced by older Auto Decor versions and this
            // editor session (no-touch and drawn on visual T layers).
            bool likelyGenerated=o->m_isNoTouch &&
                (o->m_zLayer==ZLayer::T1 || o->m_zLayer==ZLayer::T2) &&
                !terrainType && !isSlope;
            if (likelyGenerated) { ++filtered;continue; }
            if (terrainType &&
                std::abs(o->getRotation())<=.1f &&
                std::abs(o->getScaleX()-1.f)<=.15f &&
                std::abs(o->getScaleY()-1.f)<=.15f &&
                terrain.size()<kMaxBlocks) {
                terrain.push_back({pt,o->m_objectID});
                auto residue=[](float value)->int {
                    int v=int(std::lround(std::fmod(value,30.f)));
                    return (v%30+30)%30;
                };
                lattice[residue(pt.x)*30 + residue(pt.y)]++;
            } else if ((isGameplayAnchor || terrainType) && anchors.size()<kMaxBlocks) {
                if(isSlope)++slopes;
                anchors.push_back({pt,int(type)});
            } else ++filtered;
        }
        // Choose the DOMINANT lattice, not the first encountered block.
        // This fixes levels with 0/15/other offsets and off-grid decoration.
        int dominant=0;
        for(int i=1;i<900;++i)if(lattice[i]>lattice[dominant])dominant=i;
        float ox=float(dominant/30),oy=float(dominant%30);
        std::unordered_map<Cell,CCPoint,CellHash> cells;
        cells.reserve(terrain.size());
        for (auto const& candidate : terrain) {
            auto pt=candidate.pos;
            int x=int(std::lround((pt.x-ox)/30.f));
            int y=int(std::lround((pt.y-oy)/30.f));
            auto tolerance=4.75f;
            if(std::abs(pt.x-(ox+x*30.f))>tolerance ||
               std::abs(pt.y-(oy+y*30.f))>tolerance) {
                // Secondary lattices are still represented as contextual
                // background anchors, not discarded.
                if(anchors.size()<kMaxBlocks) anchors.push_back({pt,0});
                continue;
            }
            cells[{x,y}]=pt;
        }
        if(cells.empty() && anchors.empty()) {
            note("No supported geometry found. Try selecting solid blocks and slopes.");
            return;
        }
        m_fields->inspected=inspected;
        m_fields->eligible=cells.size();
        m_fields->anchorCount=anchors.size();
        std::vector<Cell> ordered;
        ordered.reserve(cells.size());
        for (auto const& entry : cells) ordered.push_back(entry.first);
        std::sort(ordered.begin(), ordered.end(), [](Cell a, Cell b) {
            return a.x == b.x ? a.y < b.y : a.x < b.x;
        });
        auto has = [&cells](int x, int y) { return cells.contains({x,y}); };
        auto exposed = [&has](Cell c) -> Neighborhood {
            return {!has(c.x,c.y+1), !has(c.x+1,c.y), !has(c.x,c.y-1),
                    !has(c.x-1,c.y), !has(c.x-1,c.y+1), !has(c.x+1,c.y+1),
                    !has(c.x+1,c.y-1), !has(c.x-1,c.y-1)};
        };
        auto p = palette(style);
        auto density = m_fields->detail;
        const float sparse = density == Detail::Clean ? 0.f :
                            density == Detail::Balanced ? .25f : .45f;
        std::vector<Piece> pieces;
        pieces.reserve(std::min(kMaxPieces, cells.size()*3 + anchors.size()));
        auto append = [&pieces](int id, CCPoint pos, float scale, float angle, ZLayer zlayer, int zorder, int ink=0) {
            if (id > 0 && pieces.size() < kMaxPieces)
                pieces.push_back({id,pos,scale,angle,zlayer,zorder,ink});
        };

        // PASS 1 — cohesive base textures: every occupied block receives
        // a tile determined by the 8-neighbor layout (edge, corner, notch,
        // pillar, isolated, interior). The underlying gameplay stays intact.
        for (auto c : ordered) {
            auto n = exposed(c);
            auto t = chooseTile(p,n,style);
            // Opaque tile backgrounds go BEHIND original collision blocks.
            append(t.id,cells.at(c),1.f,t.angle,ZLayer::B1,0,0);
        }

        // PASS 2 — controlled details with theme-specific visual language.
        // No art is scattered through open gameplay space.
        for (auto c : ordered) {
            if (pieces.size() >= kMaxPieces) break;
            auto n = exposed(c);
            auto pos = cells.at(c);
            auto r = scramble(c.x,c.y,117);
            bool surface = n.t;
            bool interior = n.exposed() == 0;
            if (style == Style::Neon) {
                // Second outline layer adds readable continuous silhouette.
                if (density != Detail::Clean && n.exposed() > 0) {
                    int id = (n.exposed()==2 && (n.t || n.b)) ? 1192 : 1191;
                    auto base = chooseTile(p,n,style);
                    if (probability(c.x,c.y,19) < (density==Detail::Rich ? .82f : .59f))
                        append(id,pos,.90f,base.angle,ZLayer::T1,1,1);
                }
                if (interior && probability(c.x,c.y,18) < sparse*.55f)
                    append(1195,pos,.55f,0,ZLayer::T2,3,2);
            } else if (style == Style::Tech) {
                // The conduit grid is consistent across adjacent cells:
                // pipes meet at deliberate cross/turns, not random points.
                auto conduit = [&has](int x,int y) -> bool {
                    return has(x,y) && (posmod(x,4)==0 || posmod(y,4)==0);
                };
                if (density != Detail::Clean && interior && conduit(c.x,c.y)) {
                    bool N=conduit(c.x,c.y+1), E=conduit(c.x+1,c.y);
                    bool S=conduit(c.x,c.y-1), W=conduit(c.x-1,c.y);
                    int degree = count(N,E,S,W);
                    int id=0; float angle=0;
                    if (degree==4) id=240;
                    else if (degree==3) {id=239; angle=!N?180:!E?270:!S?0:90;}
                    else if (degree==2 && ((N&&S)||(E&&W))) {
                        id=237; angle=(E&&W)?90:0;
                    }
                    else if (degree==2) {
                        id=238;
                        angle=(N&&W)?0:(N&&E)?90:(S&&E)?180:270;
                    }
                    else if (degree==1) {
                        id=241;
                        angle=N?0:E?90:S?180:270;
                    }
                    if (id && (density==Detail::Rich || (r % 5)!=0))
                        append(id,pos,.73f,angle,ZLayer::T1,3,1);
                }
                if (surface && probability(c.x,c.y,23) < sparse*.35f)
                    append(1005,{pos.x,pos.y+7.f},.42f,0,ZLayer::T2,5,2);
            } else if (style == Style::Ruins) {
                // Masonry seams; alternate intact bricks and broken stone,
                // preserving a visible consistent wall over multiple tiles.
                bool mortar = (posmod(c.x + 2*posmod(c.y,2),4)==0);
                if (interior && mortar && probability(c.x,c.y,34) < sparse*1.6f)
                    append(277,pos,.67f,0,ZLayer::T1,2,1);
                else if (n.exposed() > 0 && probability(c.x,c.y,35) < sparse*.7f)
                    append(278,pos,.52f,float((r>>11)%4)*90,ZLayer::T1,3,1);
                if (interior && density==Detail::Rich && probability(c.x,c.y,36)<.07f)
                    append(245,pos,.46f,0,ZLayer::T2,4,2);
            } else if (style == Style::Nature) {
                // Vegetation grows only on sky-facing tops. Alternating
                // placements produce clumps rather than a noisy grass carpet.
                bool clump = posmod(c.x + (c.y/5),3)==0;
                if (surface && clump && probability(c.x,c.y,41)<sparse*2.5f) {
                    int id = std::array<int,6>{406,407,408,940,941,942}[r%6];
                    append(id,{pos.x,pos.y+9.f},.55f,0,ZLayer::T2,5,1);
                }
                if (interior && probability(c.x,c.y,42)<sparse*.65f)
                    append(1048,pos,.54f,0,ZLayer::T1,2,1);
                if (surface && density==Detail::Rich &&
                    probability(c.x,c.y,43)<.035f)
                    append(939,{pos.x,pos.y+9.f},.55f,0,ZLayer::T2,5,2);
            }
        }

        // PASS 3 — contextual support for slopes, portals, hazards and other
        // non-square geometry. The previous mod silently ignored all of these.
        // Group into broad 120px columns to avoid repetitive clutter.
        std::unordered_map<int,std::pair<CCPoint,int>> zones;
        for(auto const& a:anchors) {
            int key=int(std::floor(a.pos.x/120.f));
            auto& zone=zones[key];
            if(zone.second==0)zone.first=a.pos;
            zone.second++;
        }
        std::vector<int> sortedZones;
        for(auto const& pair:zones)sortedZones.push_back(pair.first);
        std::sort(sortedZones.begin(),sortedZones.end());
        int backdropID=style==Style::Neon?1168:style==Style::Tech?263:
                       style==Style::Ruins?269:955;
        int highlightID=style==Style::Neon?1195:style==Style::Tech?240:
                        style==Style::Ruins?277:406;
        for(int key:sortedZones) {
            if(pieces.size()+2>=kMaxPieces)break;
            auto const& zone=zones.at(key);
            auto center=zone.first;
            auto pulse=scramble(key,int(center.y/30.f),81);
            // Place decorative supports 30px below interesting gameplay
            // features, *behind* the action, without covering the hitboxes.
            float y=center.y-30.f;
            append(backdropID,{center.x,y},.62f,float(pulse%4)*90.f,ZLayer::B3,-2,0);
            if(density==Detail::Rich || (density==Detail::Balanced && pulse%3==0)) {
                append(highlightID,{center.x,y-16.f},.32f,float((pulse>>4)%4)*90.f,
                       ZLayer::B2,-1,1);
            }
        }
        // Configure THREE NEW PRIVATE COLOR CHANNELS per style. Unlike
        // editor-only setColor these colors survive save/load of the level.
        auto* manager=layer->m_effectManager;
        auto styleIndex=static_cast<size_t>(style);
        auto paletteRGB=inkPalette(style).colors;
        auto& savedChannels=m_fields->colorChannels[styleIndex];
        if(manager) {
            for(int k=0;k<3;++k) {
                if(savedChannels[k]==0) {
                    int channel=layer->getNextColorChannel();
                    if(channel>=1 && channel<=999) {
                        auto* action=ColorAction::create(paletteRGB[k],false,0);
                        if(action) {
                            manager->setColorAction(action,channel);
                            savedChannels[k]=channel;
                        }
                    }
                }
            }
            manager->processColors();
        }
        m_fields->activeColorIDs=savedChannels;
        m_fields->activeRGB=paletteRGB;

        // Place 85 objects per frame to prevent editor stalls on big levels.
        m_fields->work = std::move(pieces);
        m_fields->liveIDs.clear();
        m_fields->liveIDs.reserve(m_fields->work.size());
        m_fields->cursor = 0;
        m_fields->created = 0;
        m_fields->rejected = 0;
        m_fields->activeStyle = style;
        m_fields->busy = true;
        this->schedule(schedule_selector(AutoDecorEditor::processWork));
        geode::log::info("AutoDecor {}: scope={}, scanned={}, eligible={}, queued={}, filtered={}",
            name(style), m_fields->scope == Scope::WholeLevel ? "ALL" : "SELECTED",
            inspected, cells.size(), m_fields->work.size(), filtered);
    }
    void processWork(float) {
        if (!m_fields->busy) return;
        auto* layer = this->m_editorLayer;
        if (!layer) {
            this->unschedule(schedule_selector(AutoDecorEditor::processWork));
            m_fields->busy = false;
            m_fields->work.clear();
            m_fields->liveIDs.clear();
            return;
        }
        size_t end = std::min(m_fields->cursor + kFrameBudget, m_fields->work.size());
        for (; m_fields->cursor < end; ++m_fields->cursor) {
            auto const& p = m_fields->work[m_fields->cursor];
            auto* obj = layer->createObject(p.id, p.pos, false);
            if (!obj) { ++m_fields->rejected; continue; }
            if (obj->m_objectType == GameObjectType::Hazard ||
                obj->m_objectType == GameObjectType::AnimatedHazard ||
                obj->m_objectType == GameObjectType::CollisionObject ||
                obj->m_objectType == GameObjectType::Modifier) {
                layer->removeObject(obj, false);
                ++m_fields->rejected;
                continue;
            }
            obj->setScale(p.scale);
            obj->setRotation(p.angle);
            obj->m_zLayer = p.layer;
            obj->m_zOrder = p.zOrder;
            obj->m_isNoTouch = true;
            // White overlay regression fix: explicitly assign each created
            // piece to the theme's persisted GD color channels.
            int primary=m_fields->activeColorIDs[std::clamp(p.ink,0,2)];
            int secondary=m_fields->activeColorIDs[1];
            if(primary>0) {
                if(obj->m_baseColor) obj->m_baseColor->m_colorID=primary;
                if(obj->m_detailColor && secondary>0) obj->m_detailColor->m_colorID=secondary;
            }
            obj->setObjectColor(m_fields->activeRGB[std::clamp(p.ink,0,2)]);
            layer->manualUpdateObjectColors(obj);
            if (obj->m_uniqueID > 0) m_fields->liveIDs.push_back(obj->m_uniqueID);
            ++m_fields->created;
        }
        if (m_fields->cursor == m_fields->work.size()) {
            this->unschedule(schedule_selector(AutoDecorEditor::processWork));
            auto n=m_fields->created;
            auto skipped=m_fields->rejected;
            auto blocks=m_fields->eligible;
            auto inspected=m_fields->inspected;
            auto theme=m_fields->activeStyle;
            auto background=m_fields->anchorCount;
            m_fields->busy=false;
            m_fields->work.clear();
            if (!m_fields->liveIDs.empty()) {
                m_fields->history.push_back(std::move(m_fields->liveIDs));
                if (m_fields->history.size() > 10)
                    m_fields->history.erase(m_fields->history.begin());
            }
            m_fields->liveIDs.clear();
            note(std::string(name(theme)) + ": found " +
                std::to_string(blocks) + " blocks / " + std::to_string(inspected) +
                " objects + " + std::to_string(background) + " context anchors, placed " + std::to_string(n) +
                ", skipped " + std::to_string(skipped) +
                ". Playtest before saving.");
        }
    }
    void revert(CCObject*) {
        if (m_fields->panel) m_fields->panel->setVisible(false);
        auto* layer = this->m_editorLayer;
        if (!layer) return;
        std::vector<int> batch;
        if (m_fields->busy) {
            this->unschedule(schedule_selector(AutoDecorEditor::processWork));
            m_fields->busy = false;
            m_fields->work.clear();
            batch = std::move(m_fields->liveIDs);
            m_fields->liveIDs.clear();
        } else {
            if (m_fields->history.empty()) {
                note("No recent Auto Decor batch to undo.");
                return;
            }
            batch = std::move(m_fields->history.back());
            m_fields->history.pop_back();
        }
        size_t deleted=0;
        for (int uniqueID : batch) {
            if (auto* obj = layer->findGameObject(uniqueID)) {
                layer->removeObject(obj,false);
                ++deleted;
            }
        }
        note("Removed " + std::to_string(deleted) + " objects from last batch.");
    }
};
