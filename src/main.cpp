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
InkPalette inkPalette(Style s, int variant) {
    auto make=[](ccColor3B a, ccColor3B b, ccColor3B c) -> InkPalette {
        return InkPalette{std::array<ccColor3B,3>{a,b,c}};
    };
    variant=((variant%4)+4)%4;
    switch(s) {
      case Style::Neon:
        switch(variant) {
          case 0: return make({17,28,46},{49,216,239},{170,77,255}); // cyan/purple
          case 1: return make({38,17,39},{255,78,194},{255,203,74}); // hot pink/gold
          case 2: return make({11,43,46},{64,254,161},{99,164,255}); // mint/blue
          default:return make({47,23,31},{255,118,77},{253,74,114}); // coral/red
        }
      case Style::Tech:
        switch(variant) {
          case 0: return make({37,56,72},{102,194,231},{255,194,91});
          case 1: return make({34,47,55},{97,241,193},{246,136,82});
          case 2: return make({37,33,59},{149,172,255},{248,88,214});
          default:return make({50,47,40},{239,190,95},{171,207,217});
        }
      case Style::Ruins:
        switch(variant) {
          case 0: return make({71,52,50},{189,112,85},{249,188,113});
          case 1: return make({55,51,63},{168,153,199},{230,204,151});
          case 2: return make({51,63,56},{140,172,123},{217,177,112});
          default:return make({76,48,47},{207,112,106},{248,202,153});
        }
      case Style::Nature:
        switch(variant) {
          case 0: return make({29,65,51},{97,188,101},{235,201,96});
          case 1: return make({42,55,73},{124,206,196},{185,154,234});
          case 2: return make({71,64,45},{187,169,87},{255,210,148});
          default:return make({46,57,65},{118,161,203},{221,164,123});
        }
    }
    return make({32,32,32},{200,200,200},{255,255,255});
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
constexpr size_t kFrameBudget = 18;
}

class $modify(AutoDecorEditor, EditorUI) {
    struct Fields {
        CCMenu* panel = nullptr;
        Detail detail = Detail::Balanced;
        Scope scope = Scope::WholeLevel;
        std::vector<std::vector<int>> history;
        std::vector<Piece> work;
        int paletteShift = 0;
        CCLabelBMFont* progressLabel = nullptr;
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
        auto* progress = CCLabelBMFont::create("AUTO DECOR: 0%", "goldFont.fnt");
        progress->setScale(.32f);
        progress->setPosition({size.width - 105.f, size.height - 27.f});
        progress->setVisible(false);
        this->addChild(progress, 600);
        m_fields->progressLabel = progress;

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
        panel->addChild(button("NEW COLORS", menu_selector(AutoDecorEditor::nextColors), 0.34f));
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
    void nextColors(CCObject*) {
        m_fields->paletteShift = (m_fields->paletteShift + 1) % 4;
        note("New color harmony selected. Generate with a theme.");
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

        // Art director: motifs are planned at 6x4 and 9x6 scales.
        // One tile per block is not enough: controlled edge strokes, depth,
        // focal ornaments, micro-details and negative space form a hierarchy.
        const float detailFactor = density == Detail::Clean ? .50f :
                                   density == Detail::Balanced ? 1.f : 1.65f;
        int maxX = ordered.empty()?0:ordered.front().x;
        int minX = maxX, maxY = ordered.empty()?0:ordered.front().y, minY=maxY;
        for(auto c:ordered) {
            minX=std::min(minX,c.x);maxX=std::max(maxX,c.x);
            minY=std::min(minY,c.y);maxY=std::max(maxY,c.y);
        }
        int variant = int(scramble(minX,maxY,int(style))%4);
        variant=(variant+m_fields->paletteShift)%4;

        // A 6x4 "artboard" uses the same palette and motif rhythm,
        // but adjacent artboards can carry different ornament types.
        auto section=[](Cell c) -> Cell {
            return {int(std::floor(float(c.x)/6.f)),
                    int(std::floor(float(c.y)/4.f))};
        };
        auto add=[&](int id, CCPoint pt, float scale, float ang,
                      ZLayer depth, int order, int ink){
            append(id,pt,scale,ang,depth,order,ink);
        };
        int groundDetail=0;
        for(auto c:ordered) {
            if(pieces.size()+10>=kMaxPieces)break;
            auto n=exposed(c);
            auto pt=cells.at(c);
            int edges=n.exposed();
            auto zone=section(c);
            uint32_t zoneSeed=scramble(zone.x,zone.y,int(style)*97);
            uint32_t h=scramble(c.x,c.y,int(style)*43+7);
            const bool focal=(posmod(c.x-zone.x*6,6)==2 &&
                              posmod(c.y-zone.y*4,4)==1);
            // FOUNDATION: selective transparent/decorative material only.
            // Never add a new collision-bearing solid block.
            if(edges==0 && (h%3)!=0) {
                int material = style==Style::Neon ? 279 :
                               style==Style::Tech ? 281 :
                               style==Style::Ruins ? 277 : 1048;
                float scl = style==Style::Neon ? .78f : .85f;
                add(material,pt,scl,float((h>>9)%4)*90.f,ZLayer::B2,-3,0);
                groundDetail++;
            }

            // EDGE LAYER: all exposed edges get a continuous material contour.
            // The two strokes are geometrically aligned and differently tinted.
            auto stroke = [&](float dx,float dy,float angle) {
                if(style==Style::Neon) {
                    add(504,{pt.x+dx,pt.y+dy},.72f,angle,ZLayer::B1,-1,0);
                    add(1292,{pt.x+dx,pt.y+dy},.64f,angle,ZLayer::T1,2,1);
                    if(detailFactor>=1.f && (h%4)!=0)
                        add(1292,{pt.x+dx*.78f,pt.y+dy*.78f},
                            .36f,angle,ZLayer::T2,3,2);
                } else if(style==Style::Tech) {
                    add(237,{pt.x+dx*.67f,pt.y+dy*.67f},.48f,angle,ZLayer::T1,2,1);
                    if((h&3)==0) add(231,{pt.x+dx*.42f,pt.y+dy*.42f},
                                    .32f,angle,ZLayer::T2,4,2);
                } else if(style==Style::Ruins) {
                    add(235,{pt.x+dx*.50f,pt.y+dy*.50f},
                        .55f,angle,ZLayer::T1,1,1);
                    if((h&3)==1) add(278,{pt.x+dx*.32f,pt.y+dy*.32f},
                                    .34f,angle,ZLayer::T2,2,2);
                } else {
                    // only exposed terrain receives vegetation
                    if(angle==0) {
                        int grass=std::array<int,4>{406,407,408,940}[h%4];
                        add(grass,{pt.x,pt.y+11.f},.70f,0,ZLayer::T2,3,1);
                        if(detailFactor>1.f && (h%5)==0)
                            add(939,{pt.x+6.f,pt.y+13.f},.45f,0,ZLayer::T2,5,2);
                    } else if((h%4)==0) {
                        add(1048,{pt.x+dx*.45f,pt.y+dy*.45f},.55f,angle,
                            ZLayer::T1,1,1);
                    }
                }
            };
            if(n.t)stroke(0,12,0);
            if(n.r)stroke(12,0,90);
            if(n.b)stroke(0,-12,180);
            if(n.l)stroke(-12,0,270);

            // THEME STRUCTURE: patterned details, not identical repeats.
            // Playable surfaces stay legible; detail density concentrates
            // inside masses and in selected architectural highlights.
            if(edges==0 && detailFactor>=1.f) {
                if(style==Style::Neon) {
                    if((posmod(c.x+2*c.y,5)==0)&&(h%5<3))
                        add(1004,pt,.48f,0,ZLayer::T1,3,1);
                    if((h%11)==0)
                        add(1887,{pt.x+5.f,pt.y-6.f},.42f,0,ZLayer::T2,5,2);
                } else if(style==Style::Tech) {
                    if((posmod(c.x,4)==0 || posmod(c.y,4)==0) && h%3!=0)
                        add(1004,pt,.48f,float((h>>3)%4)*90.f,ZLayer::T1,2,1);
                    if((h%13)==0)
                        add(87,pt,.30f,0,ZLayer::T2,5,2);
                } else if(style==Style::Ruins) {
                    if(posmod(c.x+2*c.y,3)==0)
                        add(277,pt,.54f,float(h%2)*180,ZLayer::T1,2,1);
                    if((h%13)==0)
                        add(245,{pt.x+4.f,pt.y-3.f},.45f,0,ZLayer::T2,3,0);
                } else {
                    if((h%5)<2)
                        add(1048,pt,.58f,0,ZLayer::T1,2,1);
                }
            }

            // FOCAL POINT: a coherent architectural jewel at approximately
            // every 6x4 cells. It gets several nested layers and a halo.
            // Focals only occur if neighboring blocks provide visual support.
            if(focal && edges<=1 && detailFactor>=.50f) {
                bool supported=has(c.x-1,c.y)&&has(c.x+1,c.y);
                if(supported) {
                    if(style==Style::Neon) {
                        add(1888,pt,1.30f,0,ZLayer::B1,-2,0);
                        add(236,pt,.48f,0,ZLayer::T1,7,1);
                        add(1887,pt,.27f,0,ZLayer::T2,8,2);
                        if(detailFactor>=1.f) {
                            add(232,{pt.x-16.f,pt.y},.34f,0,ZLayer::T2,8,1);
                            add(232,{pt.x+16.f,pt.y},.34f,180,ZLayer::T2,8,1);
                        }
                    } else if(style==Style::Tech) {
                        add(284,pt,.73f,0,ZLayer::B1,-2,0);
                        add(86,pt,.46f,0,ZLayer::T1,6,1);
                        add(1887,pt,.26f,0,ZLayer::T2,7,2);
                        if(detailFactor>=1.f) {
                            add(231,{pt.x-15.f,pt.y},.36f,0,ZLayer::T2,7,1);
                            add(231,{pt.x+15.f,pt.y},.36f,180,ZLayer::T2,7,1);
                        }
                    } else if(style==Style::Ruins) {
                        add(113,pt,.63f,0,ZLayer::B1,-2,0);
                        add(114,pt,.55f,0,ZLayer::T1,5,1);
                        if((zoneSeed%3)==0)
                            add(1887,pt,.45f,0,ZLayer::T2,6,2);
                    } else {
                        add(222,pt,.67f,0,ZLayer::B2,-2,0);
                        add(223,pt,.45f,0,ZLayer::B1,-1,1);
                        if((zoneSeed%3)==0)
                            add(1887,{pt.x+4.f,pt.y+4.f},.29f,0,ZLayer::T2,6,2);
                    }
                }
            }
        }

        // CONTEXT PASS: one architectural/background accent per broad zone
        // of non-square gameplay geometry. Reusing the same square tiles here
        // was the principal cause of the old monotonous scenery.
        std::unordered_map<int,std::pair<CCPoint,int>> zones;
        for(auto const& a:anchors) {
            int key=int(std::floor(a.pos.x/180.f));
            auto& z=zones[key];
            if(z.second==0)z.first=a.pos;
            z.second++;
        }
        std::vector<int> zoneIDs;
        for(auto const& q:zones)zoneIDs.push_back(q.first);
        std::sort(zoneIDs.begin(),zoneIDs.end());
        for(auto k:zoneIDs) {
            if(pieces.size()+8>=kMaxPieces)break;
            auto center=zones.at(k).first;
            uint32_t h=scramble(k,int(center.y/30),int(style)+23);
            auto at=CCPoint{center.x,center.y-38.f};
            if(style==Style::Neon) {
                add(1888,at,.95f,0,ZLayer::B3,-3,0);
                add(1887,at,.45f,0,ZLayer::B2,-2,2);
                if(detailFactor>=1.f) {
                    add(504,{at.x-13,at.y},.55f,90,ZLayer::B2,-2,1);
                    add(504,{at.x+13,at.y},.55f,90,ZLayer::B2,-2,1);
                }
            } else if(style==Style::Tech) {
                add(283,at,.70f,float((h%4)*90),ZLayer::B3,-3,0);
                add(86,at,.37f,0,ZLayer::B2,-2,1);
                if(detailFactor>=1.f)
                    add(1887,at,.22f,0,ZLayer::B1,-1,2);
            } else if(style==Style::Ruins) {
                add(113,at,.60f,0,ZLayer::B3,-3,0);
                add(110,{at.x,at.y+16.f},.35f,0,ZLayer::B2,-2,1);
                if(detailFactor>1.f)
                    add(114,{at.x+12.f,at.y-11.f},.30f,0,ZLayer::B2,-1,2);
            } else {
                add(222,at,.75f,0,ZLayer::B3,-3,0);
                add(223,{at.x+10.f,at.y+6.f},.50f,0,ZLayer::B2,-2,1);
                if(detailFactor>1.f)
                    add(1887,{at.x-12.f,at.y-5.f},.28f,0,ZLayer::B1,-1,2);
            }
        }
                // Each generation gets its OWN persistent color channels, so changing
        // palette on the next run never recolors earlier batches.
        std::array<int,3> channels{0,0,0};
        auto rgb=inkPalette(style,variant).colors;
        auto* manager=layer->m_effectManager;
        if(manager) {
            for(int k=0;k<3;++k) {
                int channel=layer->getNextColorChannel();
                if(channel>0 && channel<=999) {
                    auto* action=ColorAction::create(rgb[k],false,0);
                    if(action) {
                        manager->setColorAction(action,channel);
                        manager->processColors();
                        channels[k]=channel;
                    }
                }
            }
        }
        m_fields->activeColorIDs=channels;
        m_fields->activeRGB=rgb;
        // Place 85 objects per frame to prevent editor stalls on big levels.
        m_fields->work = std::move(pieces);
        m_fields->liveIDs.clear();
        m_fields->liveIDs.reserve(m_fields->work.size());
        m_fields->cursor = 0;
        m_fields->created = 0;
        m_fields->rejected = 0;
        m_fields->activeStyle = style;
        m_fields->busy = true;
        if(m_fields->progressLabel) {
            m_fields->progressLabel->setString("AUTO DECOR: 0%");
            m_fields->progressLabel->setVisible(true);
        }
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
            if(m_fields->progressLabel)m_fields->progressLabel->setVisible(false);
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
                obj->m_objectType == GameObjectType::Modifier ||
                obj->m_objectType == GameObjectType::Solid ||
                obj->m_objectType == GameObjectType::Breakable ||
                obj->m_objectType == GameObjectType::Slope) {
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
        if(m_fields->progressLabel && !m_fields->work.empty()) {
            int progress=int(100.f*float(m_fields->cursor)/float(m_fields->work.size()));
            std::string msg="AUTO DECOR: "+std::to_string(progress)+"%";
            m_fields->progressLabel->setString(msg.c_str());
        }
        if (m_fields->cursor == m_fields->work.size()) {
            if(m_fields->progressLabel)m_fields->progressLabel->setVisible(false);
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
            if(m_fields->progressLabel)m_fields->progressLabel->setVisible(false);
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
