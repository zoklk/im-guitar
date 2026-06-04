#pragma once
#include "imgui.h"
#include "view/Panel.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <cmath>
#include <map>
#include "../common/TextureLoader.h"

inline std::string g_selectedMachineId = "";

enum class Anchor { Left, Right, Top, Bottom };

struct ProductImage {
    GLuint textureID = 0; // OpenGL 텍스처 ID
    int width = 0;       // 원본 너비
    int height = 0;      // 원본 높이
};

struct NodeInfo {
    ImVec2 pos;
    ImVec4 baseColor;
    std::string displayName;
};

struct ConvLayout {
    std::string startId;
    std::string endId;
    std::string convId;
    Anchor startAnchor = Anchor::Right; 
    Anchor endAnchor = Anchor::Left;    
};

class FactoryFloorPanel : public Panel {
private:
    std::unordered_map<std::string, NodeInfo> nodes_;
    std::vector<ConvLayout> lines_;
    // 물품 ID와 이미지를 연결하는 맵
    std::map<std::string, ProductImage> productImages;
    bool imagesLoaded = false;

    void LoadProductImages() {
        if (imagesLoaded) return;

        std::string baseMainPath = "../assets/images/"; 

        std::vector<std::string> productIDs = {
            "WOOD_BODY", "WOOD_NECK", "WOOD_HEAD", "BODY_RAW", "BODY_PAINTED",
            "NECK_RAW", "HEAD_RAW", "PICKUP", "BRIDGE", "ASSEMBLY_BODY", "ELEC", "GUITAR",
            "TECHNICIAN"
        };

        for (const auto& id : productIDs) {
            ProductImage img;
            std::string filename = baseMainPath + id + ".png"; 
            if (TextureLoader::LoadTextureFromFile(filename.c_str(), &img.textureID, &img.width, &img.height)) {
                productImages[id] = img;
            } else {
                std::printf("Failed to load image for: %s (Check path: %s)\n", id.c_str(), filename.c_str());
            }
        }
        imagesLoaded = true;
    }

    void initLayout() {
        if (!nodes_.empty()) return;

        ImVec4 cDarkRed = ImVec4(0.6f, 0.15f, 0.15f, 1.0f);
        ImVec4 cOrange  = ImVec4(0.85f, 0.5f, 0.3f, 1.0f); 
        ImVec4 cYellow  = ImVec4(0.95f, 0.75f, 0.2f, 1.0f);
        ImVec4 cGreen   = ImVec4(0.35f, 0.75f, 0.4f, 1.0f);
        ImVec4 cGrey    = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);  
        ImVec4 cPurple  = ImVec4(0.7f, 0.65f, 0.8f, 1.0f); 
        ImVec4 cRed     = ImVec4(0.9f, 0.2f, 0.25f, 1.0f); 

        float col1 = 30.0f;
        float col2 = 280.0f;
        float col3 = 530.0f;
        float col4 = 780.0f;
        float col5 = 1030.0f;

        float midY = 245.0f;

        nodes_["SPN_WOOD_BODY"] = {ImVec2(col1, 50), cDarkRed, "Wood Spawner"};
        nodes_["SPN_WOOD_NECK"] = {ImVec2(col1, 180), cDarkRed, "Wood Spawner"};
        nodes_["SPN_WOOD_HEAD"] = {ImVec2(col1, 310), cDarkRed, "Wood Spawner"};
        
        nodes_["MCH_BODY_CUT"]  = {ImVec2(col2, 50), cOrange, "Body Cutter"};
        nodes_["MCH_NECK_CUT"]  = {ImVec2(col2, 180), cOrange, "Neck Cutter"};
        nodes_["MCH_HEAD_CUT"]  = {ImVec2(col2, 310), cOrange, "Head Cutter"};
        
        nodes_["MCH_PAINT"]     = {ImVec2(col3, 50), cYellow, "Painter"};
        nodes_["MCH_BODY_ASM"]  = {ImVec2(col3, midY), cGreen, "Body Assembler"};
        nodes_["SPN_PICKUP"]    = {ImVec2(col3, 380), cGrey, "Pickup Spawner"};
        nodes_["SPN_BRIDGE"]    = {ImVec2(col3, 510), cGrey, "Bridge Spawner"};
        
        nodes_["MCH_PART_ASM"]  = {ImVec2(col4, midY), cGreen, "Part Assembler"};
        nodes_["MCH_ELEC"]      = {ImVec2(col4, 445), cPurple, "Elec Part Collector"};
        
        nodes_["MCH_PACK"]      = {ImVec2(col5, midY), cRed, "Packager"};

        lines_ = {
            {"SPN_WOOD_BODY", "MCH_BODY_CUT", "CONV_WOOD_BODY"},
            {"SPN_WOOD_NECK", "MCH_NECK_CUT", "CONV_WOOD_NECK"},
            {"SPN_WOOD_HEAD", "MCH_HEAD_CUT", "CONV_WOOD_HEAD"},
            {"MCH_BODY_CUT",  "MCH_PAINT",    "CONV_BODY_RAW"},
            {"MCH_PAINT",     "MCH_BODY_ASM", "CONV_BODY_PAINTED", Anchor::Bottom, Anchor::Top},
            {"MCH_NECK_CUT",  "MCH_BODY_ASM", "CONV_NECK"},
            {"MCH_HEAD_CUT",  "MCH_BODY_ASM", "CONV_HEAD"},
            {"SPN_PICKUP",    "MCH_ELEC",     "CONV_PICKUP"},
            {"SPN_BRIDGE",    "MCH_ELEC",     "CONV_BRIDGE"},
            {"MCH_BODY_ASM",  "MCH_PART_ASM", "CONV_ASMBODY"},
            {"MCH_ELEC",      "MCH_PART_ASM", "CONV_ELEC", Anchor::Top, Anchor::Bottom},
            {"MCH_PART_ASM",  "MCH_PACK",     "CONV_GUITAR"}
        };
    }

    ImVec2 getAnchorOffset(Anchor anchor) {
        float w = 120.0f; float h = 90.0f;
        switch(anchor) {
            case Anchor::Left:   return ImVec2(0, h / 2.0f);
            case Anchor::Right:  return ImVec2(w, h / 2.0f);
            case Anchor::Top:    return ImVec2(w / 2.0f, 0);
            case Anchor::Bottom: return ImVec2(w / 2.0f, h);
        }
        return ImVec2(0,0);
    }

    void DrawMachineNode(const MachineSnap& mSnap, const NodeInfo& info) {
        ImVec4 bgColor = info.baseColor;
        const char* statusText = "IDLE";

        if (mSnap.health == 0) {
            bgColor = ImVec4(0.2f, 0.0f, 0.0f, 1.0f); 
            statusText = "BROKEN!";
        } else if (mSnap.suspended) {
            statusText = "SUSPENDED";
        } else if (!mSnap.currentProduct.empty()) {
            statusText = "WORKING";
            bgColor.x = std::min(1.0f, bgColor.x + 0.15f);
            bgColor.y = std::min(1.0f, bgColor.y + 0.15f);
            bgColor.z = std::min(1.0f, bgColor.z + 0.15f);
        }

        ImGui::SetCursorPos(info.pos);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bgColor);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 15.0f); 
        
        ImGui::BeginChild(mSnap.id.c_str(), ImVec2(120, 90), true, ImGuiWindowFlags_NoScrollbar);
        
        bool isSelected = (g_selectedMachineId == mSnap.id);
        if (ImGui::Selectable(info.displayName.c_str(), isSelected, 0, ImVec2(110, 15))) {
            g_selectedMachineId = mSnap.id;
        }

        ImGui::Separator();
        
        ImGui::Text("%s", statusText);
        ImGui::ProgressBar(mSnap.health / 10.0f, ImVec2(-1, 0), "");
        
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    void DrawConveyor(ImVec2 startPos, ImVec2 endPos, Anchor startAnchor, Anchor endAnchor, const ConveyorSnap* cSnap) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 windowPos = ImGui::GetWindowPos();
        float scrollX = ImGui::GetScrollX();
        float scrollY = ImGui::GetScrollY();

        ImVec2 sOffsetOrig = getAnchorOffset(startAnchor);
        ImVec2 eOffsetOrig = getAnchorOffset(endAnchor);

        ImVec2 p1_orig = ImVec2(startPos.x + sOffsetOrig.x + windowPos.x - scrollX, startPos.y + sOffsetOrig.y + windowPos.y - scrollY);
        ImVec2 p2_orig = ImVec2(endPos.x + eOffsetOrig.x + windowPos.x - scrollX, endPos.y + eOffsetOrig.y + windowPos.y - scrollY);

        float dx = p2_orig.x - p1_orig.x;
        float dy = p2_orig.y - p1_orig.y;
        float len = std::sqrt(dx*dx + dy*dy);
        
        ImVec2 p1_ext = p1_orig;
        ImVec2 p2_ext = p2_orig;
        
        if (len > 0.0f) {
            float nx = dx / len;
            float ny = dy / len;
            p1_ext.x -= nx * 22.0f;
            p1_ext.y -= ny * 22.0f;
            p2_ext.x += nx * 22.0f;
            p2_ext.y += ny * 22.0f;
        }

        drawList->AddLine(p1_ext, p2_ext, IM_COL32(160, 160, 160, 255), 26.0f); 
        drawList->AddLine(p1_ext, p2_ext, IM_COL32(110, 110, 110, 255), 20.0f); 

        int length = (cSnap != nullptr && cSnap->length > 0) ? cSnap->length : 5;
        int currentItems = 0;

        for (int i = 0; i < length; ++i) {
            float t = (i + 0.5f) / length; 
            ImVec2 slotPos = ImVec2(p1_orig.x + dx * t, p1_orig.y + dy * t);

            bool hasItem = (cSnap != nullptr && i < cSnap->slots.size() && cSnap->slots[i].has_value());
            if (hasItem) {
                currentItems++;
                
                std::string imageId;
                float imgWidth = 24.0f;
                float imgHeight = 24.0f;
                float angleOffset = 0.0f; 

                switch (cSnap->slots[i]->type) {
                    case ProductType::RawWood:
                        imageId = "WOOD_BODY"; break;
                    case ProductType::HeadPart:
                        imageId = "HEAD_RAW"; imgWidth = 18.0f; imgHeight = 20.0f; angleOffset = 3.141592f / 2.0f; break;
                    case ProductType::NeckPart:
                        imageId = "NECK_RAW"; imgWidth = 10.0f; imgHeight = 30.0f; angleOffset = 3.141592f / 2.0f; break;
                    case ProductType::BodyPart:
                        imageId = cSnap->slots[i]->isPainted ? "BODY_PAINTED" : "BODY_RAW"; 
                        imgWidth = 18.0f; imgHeight = 22.0f; angleOffset = 3.141592f / 2.0f; break;
                    case ProductType::Bridge:
                        imageId = "BRIDGE"; imgWidth = 16.0f; imgHeight = 16.0f; break;
                    case ProductType::Pickup:
                        imageId = "PICKUP"; imgWidth = 16.0f; imgHeight = 16.0f; break;
                    case ProductType::ElecPartSet:
                        imageId = "ELEC"; imgWidth = 16.0f; imgHeight = 16.0f; break;
                    case ProductType::AssembledBody:
                        imageId = "ASSEMBLY_BODY"; imgWidth = 18.0f; imgHeight = 40.0f; angleOffset = 3.141592f / 2.0f; break;
                    case ProductType::FinishedGuitar:
                        imageId = "GUITAR"; imgWidth = 18.0f; imgHeight = 40.0f; angleOffset = 3.141592f / 2.0f; break;
                    default:
                        imageId = "UNKNOWN"; break;
                }

                auto it = productImages.find(imageId);
                
                if (it != productImages.end() && it->second.textureID != 0) {
                    const ProductImage& img = it->second;
                    
                    float convAngle = std::atan2(dy, dx); 
                    float totalAngle = convAngle + angleOffset;

                    float cos_a = std::cos(totalAngle);
                    float sin_a = std::sin(totalAngle);

                    float hw = imgWidth / 2.0f;
                    float hh = imgHeight / 2.0f;

                    ImVec2 corners[4] = {
                        ImVec2(-hw, -hh), ImVec2( hw, -hh),
                        ImVec2( hw,  hh), ImVec2(-hw,  hh)
                    };

                    ImVec2 p[4];
                    for (int k = 0; k < 4; ++k) {
                        p[k].x = slotPos.x + (corners[k].x * cos_a - corners[k].y * sin_a);
                        p[k].y = slotPos.y + (corners[k].x * sin_a + corners[k].y * cos_a);
                    }

                    drawList->AddImageQuad((void*)(intptr_t)img.textureID, p[0], p[1], p[2], p[3]);
                } else {
                    drawList->AddRectFilled(ImVec2(slotPos.x - 7, slotPos.y - 7), ImVec2(slotPos.x + 7, slotPos.y + 7), IM_COL32(180, 100, 50, 255), 2.0f);
                    drawList->AddRect(ImVec2(slotPos.x - 7, slotPos.y - 7), ImVec2(slotPos.x + 7, slotPos.y + 7), IM_COL32(50, 20, 0, 255), 2.0f);
                }
            } else {
                drawList->AddCircleFilled(slotPos, 2.0f, IM_COL32(140, 140, 140, 255));
            }
        } // for 루프 끝

        // ─── 텍스트(라벨) ───
        char label[16];
        snprintf(label, sizeof(label), "%d/%d", currentItems, length);
        
        float convAngle = std::atan2(dy, dx);
        float textAngle = convAngle;
        
        // 세로 방향 컨베이어인 경우 텍스트 각도를 0(가로)으로 고정
        if (std::abs(dx) < 1.0f) {
            textAngle = 0.0f;
        } else if (std::cos(convAngle) < 0) {
            // 왼쪽으로 가는 경우 글씨가 뒤집히지 않게 180도 회전
            textAngle += 3.141592f; 
        }

        ImVec2 textSize = ImGui::CalcTextSize(label);
        
        // 텍스트를 컨베이어 위쪽으로
        float nx = -dy / len; 
        float ny = dx / len;
        
        if (std::abs(dx) < 1.0f) {
            nx = 1.0f;
            ny = 0.0f;
        } else if (ny > 0) {
            nx = -nx;
            ny = -ny;
        }
        
        // 거리를 28.0f로 설정하여 벨트 위에 예쁘게 얹히도록 계산
        ImVec2 mid = ImVec2(p1_orig.x + dx * 0.5f + nx * 28.0f, p1_orig.y + dy * 0.5f + ny * 28.0f);

        // ── 버텍스 회전 마법 시작 ──
        int vtx_start = drawList->VtxBuffer.Size;

        ImVec2 pMin = ImVec2(mid.x - textSize.x / 2 - 4, mid.y - textSize.y / 2 - 2);
        ImVec2 pMax = ImVec2(mid.x + textSize.x / 2 + 4, mid.y + textSize.y / 2 + 2);
        drawList->AddRectFilled(pMin, pMax, IM_COL32(255, 255, 255, 230), 4.0f);
        drawList->AddRect(pMin, pMax, IM_COL32(100, 100, 100, 255), 4.0f);
        drawList->AddText(ImVec2(mid.x - textSize.x / 2, mid.y - textSize.y / 2), IM_COL32(0, 0, 0, 255), label);

        int vtx_end = drawList->VtxBuffer.Size;

        float cos_a = std::cos(textAngle);
        float sin_a = std::sin(textAngle);
        for (int i = vtx_start; i < vtx_end; i++) {
            ImVec2& p = drawList->VtxBuffer[i].pos;
            p.x -= mid.x;
            p.y -= mid.y; 
            float rotated_x = (p.x * cos_a) - (p.y * sin_a);
            float rotated_y = (p.x * sin_a) + (p.y * cos_a); 
            p.x = rotated_x + mid.x;
            p.y = rotated_y + mid.y; 
        }
    } // DrawConveyor 함수 끝

    void DrawTechnicianManager(const FactorySnap& snap) {
        ImGui::SetCursorPos(ImVec2(1180, 420));
        
        ImGui::BeginChild("TechManager", ImVec2(200, 240), true);
        ImGui::Text("Technician Manager");
        ImGui::Separator();
        
        auto it = productImages.find("TECHNICIAN");
        bool hasTechImage = (it != productImages.end() && it->second.textureID != 0);

        for (const auto& tech : snap.technicians) {
            if (hasTechImage) {
                ImGui::Image((void*)(intptr_t)it->second.textureID, ImVec2(50, 50));            
            } else {
                ImGui::Bullet();
            }
            
            // 이름과 상태 텍스트
            const char* stateMsg = tech.targetMachineId.has_value() ? "Working" : "Idle";
            
            std::string displayName = tech.id;
            if (tech.id == "TECH_1") {
                displayName = "jincheol";
            } else if (tech.id == "TECH_2") {
                displayName = "jaeyong";
            }
            ImGui::Text("%s (%s)", displayName.c_str(), stateMsg);
            
            if (tech.targetMachineId.has_value()) {
                ImGui::Text("    -> %s", tech.targetMachineId.value().c_str());
            }
            
            ImGui::Spacing();
            ImGui::Separator();
        }
        ImGui::EndChild();
    }

public:
    FactoryFloorPanel() { initLayout(); }

    ~FactoryFloorPanel() {
        for (auto& pair : productImages) {
            if (pair.second.textureID != 0) {
                // OpenGL 텍스처 메모리 해제
                GLuint tex = pair.second.textureID;
                glDeleteTextures(1, &tex); 
            }
        }
    }

    void render(const FactorySnap& snap, MachineCmd& cmd) override {
        LoadProductImages(); 
        ImGui::SetNextWindowSize(ImVec2(1400, 650), ImGuiCond_FirstUseEver);
        ImGui::Begin("Factory Floor", nullptr, ImGuiWindowFlags_AlwaysHorizontalScrollbar);

        for (const auto& line : lines_) {
            const ConveyorSnap* foundConv = nullptr;
            for (const auto& c : snap.conveyors) {
                if (c.id == line.convId) { foundConv = &c; break; }
            }
            if (foundConv != nullptr) {
                DrawConveyor(nodes_[line.startId].pos, nodes_[line.endId].pos, line.startAnchor, line.endAnchor, foundConv);
            }
        }

        for (const auto& mSnap : snap.machines) {
            auto it = nodes_.find(mSnap.id);
            if (it != nodes_.end()) {
                DrawMachineNode(mSnap, it->second);
            }
        }

        DrawTechnicianManager(snap);
        ImGui::End();
    }
};