#pragma once
#include "imgui.h"
#include "view/Panel.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <cmath>
<<<<<<< HEAD

enum class Anchor { Left, Right, Top, Bottom };

=======
#include <map>
#include "../common/TextureLoader.h"

inline std::string g_selectedMachineId = "";

enum class Anchor { Left, Right, Top, Bottom };

struct ProductImage {
    GLuint textureID = 0; // OpenGL 텍스처 ID
    int width = 0;       // 원본 너비
    int height = 0;      // 원본 높이
};

>>>>>>> e45f6f6efe8e8e0187b65db4835850ce90aa6766
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
<<<<<<< HEAD
=======
    // 물품 ID와 이미지를 연결하는 맵
    std::map<std::string, ProductImage> productImages;
    bool imagesLoaded = false;

    std::map<std::string, GLuint> machineTextures_;
    bool machineTexturesLoaded_ = false;

    void LoadMachineTextures() {
        if (machineTexturesLoaded_) return;
        
        std::map<std::string, std::string> files = {
            {"Wood Spawner", "Wood Spawner.png"},
            {"Body Cutter", "Body Cutter.png"},
            {"Neck Cutter", "Neck Cutter.png"},
            {"Head Cutter", "Head Cutter.png"},
            {"Painter", "Painter.png"},
            {"Body Assembler", "Body Assembler.png"},
            {"Pickup Spawner", "Pickup Spawner.png"},
            {"Bridge Spawner", "Bridge Spawner.png"},
            {"Part Assembler", "Part Assembler.png"},
            {"Elec Part Collector", "Elec Part Collector.png"},
            {"Packager", "Packager.png"}
        };

        for (const auto& pair : files) {
            GLuint tex = 0; int w, h;
            std::string path = "../assets/images/" + pair.second; 
            
            if (TextureLoader::LoadTextureFromFile(path.c_str(), &tex, &w, &h)) {
                machineTextures_[pair.first] = tex;
            } else {
                std::string altPath = "../assets/images/" + pair.first;
                for(char& c : altPath) { if(c == ' ') c = '_'; }
                altPath += ".png";
                if (TextureLoader::LoadTextureFromFile(altPath.c_str(), &tex, &w, &h)) {
                    machineTextures_[pair.first] = tex;
                }
            }
        }
        machineTexturesLoaded_ = true;
    }

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
>>>>>>> e45f6f6efe8e8e0187b65db4835850ce90aa6766

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

<<<<<<< HEAD
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
=======
        float midY = 205.0f;

        nodes_["SPN_WOOD_BODY"] = {ImVec2(col1, 10), cDarkRed, "Wood Spawner"};
        nodes_["SPN_WOOD_NECK"] = {ImVec2(col1, 140), cDarkRed, "Wood Spawner"};
        nodes_["SPN_WOOD_HEAD"] = {ImVec2(col1, 270), cDarkRed, "Wood Spawner"};
        
        nodes_["MCH_BODY_CUT"]  = {ImVec2(col2, 10), cOrange, "Body Cutter"};
        nodes_["MCH_NECK_CUT"]  = {ImVec2(col2, 140), cOrange, "Neck Cutter"};
        nodes_["MCH_HEAD_CUT"]  = {ImVec2(col2, 270), cOrange, "Head Cutter"};
        
        nodes_["MCH_PAINT"]     = {ImVec2(col3, 10), cYellow, "Painter"};
        nodes_["MCH_BODY_ASM"]  = {ImVec2(col3, midY), cGreen, "Body Assembler"};
        nodes_["SPN_PICKUP"]    = {ImVec2(col3, 340), cGrey, "Pickup Spawner"};
        nodes_["SPN_BRIDGE"]    = {ImVec2(col3, 470), cGrey, "Bridge Spawner"};
        
        nodes_["MCH_PART_ASM"]  = {ImVec2(col4, midY), cGreen, "Part Assembler"};
        nodes_["MCH_ELEC"]      = {ImVec2(col4, 405), cPurple, "Elec Part Collector"};
>>>>>>> e45f6f6efe8e8e0187b65db4835850ce90aa6766
        
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
<<<<<<< HEAD
        float w = 120.0f; float h = 90.0f;
=======
        float w = 120.0f; 
        float h = 115.0f; 
>>>>>>> e45f6f6efe8e8e0187b65db4835850ce90aa6766
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
<<<<<<< HEAD
        const char* statusText = "IDLE";

        if (mSnap.health == 0) {
            bgColor = ImVec4(0.2f, 0.0f, 0.0f, 1.0f); 
            statusText = "BROKEN!";
        } else if (mSnap.suspended) {
            statusText = "SUSPENDED";
        } else if (!mSnap.currentProduct.empty()) {
            statusText = "WORKING";
=======
        bgColor.x *= 0.25f;
        bgColor.y *= 0.25f;
        bgColor.z *= 0.25f;

        const char* statusText = "IDLE";
        ImVec4 statusColor = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); // 텍스트 색상

        if (mSnap.health == 0) {
            bgColor = ImVec4(0.4f, 0.1f, 0.1f, 1.0f); 
            statusText = "BROKEN!";
            statusColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        } else if (mSnap.suspended) {
            statusText = "SUSPENDED";
            statusColor = ImVec4(0.9f, 0.7f, 0.0f, 1.0f);
        } else if (!mSnap.currentProduct.empty()) {
            statusText = "WORKING";
            statusColor = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
>>>>>>> e45f6f6efe8e8e0187b65db4835850ce90aa6766
            bgColor.x = std::min(1.0f, bgColor.x + 0.15f);
            bgColor.y = std::min(1.0f, bgColor.y + 0.15f);
            bgColor.z = std::min(1.0f, bgColor.z + 0.15f);
        }

        ImGui::SetCursorPos(info.pos);
        
        // 패널 배경 처리 (불투명하게)
        bgColor.w = 1.0f; 
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bgColor);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f); 
        
        ImGui::BeginChild(mSnap.id.c_str(), ImVec2(120, 115), true, ImGuiWindowFlags_NoScrollbar);        
        
        bool isSelected = (g_selectedMachineId == mSnap.id);
        if (ImGui::Selectable(info.displayName.c_str(), isSelected, 0, ImVec2(110, 15))) {
            g_selectedMachineId = mSnap.id;
        }

        ImGui::Separator();
        
        if (machineTextures_.find(info.displayName) != machineTextures_.end() && machineTextures_[info.displayName] != 0) {
            float newWidth = 64.0f;
            float newHeight = 48.0f;
            
            ImGui::SetCursorPosX(28.0f); 
            
            ImGui::Image((void*)(intptr_t)machineTextures_[info.displayName], ImVec2(newWidth, newHeight)); 
        } else {
            ImGui::Dummy(ImVec2(64, 48)); // 이미지 로드 실패 시에도 크기 맞춤
        }

        // 상태 텍스트 예쁘게 중앙 정렬
        float textWidth = ImGui::CalcTextSize(statusText).x;
        ImGui::SetCursorPosX((120.0f - textWidth) * 0.5f);
        ImGui::TextColored(statusColor, "%s", statusText);

        // 체력바
        if (mSnap.health == 0) {
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
        }
        
        // maxHealth를 기준으로 얇은 체력바 렌더링
        float hpRatio = (mSnap.maxHealth > 0) ? (mSnap.health / (float)mSnap.maxHealth) : 0.0f;
        ImGui::ProgressBar(hpRatio, ImVec2(-1, 4), ""); 
        ImGui::PopStyleColor();
        
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

<<<<<<< HEAD
        drawList->AddLine(p1_ext, p2_ext, IM_COL32(160, 160, 160, 255), 26.0f); 
        drawList->AddLine(p1_ext, p2_ext, IM_COL32(110, 110, 110, 255), 20.0f); 

        int length = (cSnap != nullptr && cSnap->length > 0) ? cSnap->length : 5;
        int currentItems = 0;

=======
        // 아이템 개수 파악 및 병목 현상 감지
        int length = (cSnap != nullptr && cSnap->length > 0) ? cSnap->length : 5;
        int currentItems = 0;
        if (cSnap != nullptr) {
            for (int i = 0; i < length; ++i) {
                if (i < cSnap->slots.size() && cSnap->slots[i].has_value()) {
                    currentItems++;
                }
            }
        }
        bool isBottleneck = (currentItems == length && length > 0);

        // 1. 벨트 선 색상
        drawList->AddLine(p1_ext, p2_ext, IM_COL32(160, 160, 160, 255), 26.0f); 
        drawList->AddLine(p1_ext, p2_ext, IM_COL32(110, 110, 110, 255), 20.0f); 

        // ─── 아이템 그리기 ───
>>>>>>> e45f6f6efe8e8e0187b65db4835850ce90aa6766
        for (int i = 0; i < length; ++i) {
            float t = (i + 0.5f) / length; 
            ImVec2 slotPos = ImVec2(p1_orig.x + dx * t, p1_orig.y + dy * t);

            bool hasItem = (cSnap != nullptr && i < cSnap->slots.size() && cSnap->slots[i].has_value());
            if (hasItem) {
<<<<<<< HEAD
                currentItems++;
                drawList->AddRectFilled(ImVec2(slotPos.x - 7, slotPos.y - 7), ImVec2(slotPos.x + 7, slotPos.y + 7), IM_COL32(180, 100, 50, 255), 2.0f);
                drawList->AddRect(ImVec2(slotPos.x - 7, slotPos.y - 7), ImVec2(slotPos.x + 7, slotPos.y + 7), IM_COL32(50, 20, 0, 255), 2.0f);
=======
                std::string imageId;
                float imgWidth = 18.0f, imgHeight = 18.0f, angleOffset = 0.0f; 

                switch (cSnap->slots[i]->type) {
                    case ProductType::RawWood: imageId = "WOOD_BODY"; break;
                    case ProductType::HeadPart: imageId = "HEAD_RAW"; imgWidth = 18.0f; imgHeight = 20.0f; angleOffset = 3.141592f / 2.0f; break;
                    case ProductType::NeckPart: imageId = "NECK_RAW"; imgWidth = 10.0f; imgHeight = 30.0f; angleOffset = 3.141592f / 2.0f; break;
                    case ProductType::BodyPart: imageId = cSnap->slots[i]->isPainted ? "BODY_PAINTED" : "BODY_RAW"; imgWidth = 18.0f; imgHeight = 22.0f; angleOffset = 3.141592f / 2.0f; break;
                    case ProductType::Bridge: imageId = "BRIDGE"; imgWidth = 16.0f; imgHeight = 16.0f; break;
                    case ProductType::Pickup: imageId = "PICKUP"; imgWidth = 16.0f; imgHeight = 16.0f; break;
                    case ProductType::ElecPartSet: imageId = "ELEC"; imgWidth = 16.0f; imgHeight = 16.0f; break;
                    case ProductType::AssembledBody: imageId = "ASSEMBLY_BODY"; imgWidth = 18.0f; imgHeight = 40.0f; angleOffset = 3.141592f / 2.0f; break;
                    case ProductType::FinishedGuitar: imageId = "GUITAR"; imgWidth = 18.0f; imgHeight = 40.0f; angleOffset = 3.141592f / 2.0f; break;
                    default: imageId = "UNKNOWN"; break;
                }

                auto it = productImages.find(imageId);
                if (it != productImages.end() && it->second.textureID != 0) {
                    const ProductImage& img = it->second;
                    float convAngle = std::atan2(dy, dx); 
                    float totalAngle = convAngle + angleOffset;
                    float cos_a = std::cos(totalAngle), sin_a = std::sin(totalAngle);
                    float hw = imgWidth / 2.0f, hh = imgHeight / 2.0f;

                    ImVec2 corners[4] = { ImVec2(-hw, -hh), ImVec2( hw, -hh), ImVec2( hw,  hh), ImVec2(-hw,  hh) };
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
>>>>>>> e45f6f6efe8e8e0187b65db4835850ce90aa6766
            } else {
                drawList->AddCircleFilled(slotPos, 2.0f, IM_COL32(140, 140, 140, 255));
            }
        }

<<<<<<< HEAD
        char label[16];
        snprintf(label, sizeof(label), "%d/%d", currentItems, length);
        ImVec2 mid = ImVec2(p1_orig.x + dx * 0.5f, p1_orig.y + dy * 0.5f);
        
        if (std::abs(dx) < 10.0f) { 
            mid.x += 35; 
        } else {
            mid.y -= 25; 
        }
        
        ImVec2 textSize = ImGui::CalcTextSize(label);
        drawList->AddRectFilled(ImVec2(mid.x - textSize.x / 2 - 4, mid.y - textSize.y / 2 - 2), 
                                ImVec2(mid.x + textSize.x / 2 + 4, mid.y + textSize.y / 2 + 2), 
                                IM_COL32(255, 255, 255, 230), 4.0f);
        drawList->AddRect(ImVec2(mid.x - textSize.x / 2 - 4, mid.y - textSize.y / 2 - 2), 
                          ImVec2(mid.x + textSize.x / 2 + 4, mid.y + textSize.y / 2 + 2), 
                          IM_COL32(100, 100, 100, 255), 4.0f);
        drawList->AddText(ImVec2(mid.x - textSize.x / 2, mid.y - textSize.y / 2), IM_COL32(0, 0, 0, 255), label);
    }

    void DrawTechnicianManager(const FactorySnap& snap) {
        ImGui::SetCursorPos(ImVec2(1180, 420));
        ImGui::BeginChild("TechManager", ImVec2(180, 140), true);
        ImGui::Text("Technician Manager");
        ImGui::Separator();
        for (const auto& tech : snap.technicians) {
            const char* stateMsg = tech.targetMachineId.has_value() ? "Working" : "Idle";
            ImGui::BulletText("%s (%s)", tech.id.c_str(), stateMsg);
            if (tech.targetMachineId.has_value()) {
                ImGui::Text(" -> %s", tech.targetMachineId.value().c_str());
            }
=======
        // ─── 2. 텍스트 라벨 ───
        char label[16];
        snprintf(label, sizeof(label), "%d/%d", currentItems, length);
        
        float convAngle = std::atan2(dy, dx);
        float textAngle = (std::abs(dx) < 1.0f) ? 0.0f : ((std::cos(convAngle) < 0) ? convAngle + 3.141592f : convAngle);
        ImVec2 textSize = ImGui::CalcTextSize(label);
        
        float nx = -dy / len, ny = dx / len;
        if (std::abs(dx) < 1.0f) { nx = 1.0f; ny = 0.0f; } else if (ny > 0) { nx = -nx; ny = -ny; }
        
        ImVec2 mid = ImVec2(p1_orig.x + dx * 0.5f + nx * 28.0f, p1_orig.y + dy * 0.5f + ny * 28.0f);

        ImU32 labelBgColor = isBottleneck ? IM_COL32(255, 200, 200, 230) : IM_COL32(255, 255, 255, 230);
        ImU32 labelBorderColor = isBottleneck ? IM_COL32(200, 50, 50, 255) : IM_COL32(100, 100, 100, 255);
        ImU32 labelTextColor = isBottleneck ? IM_COL32(200, 0, 0, 255) : IM_COL32(0, 0, 0, 255);

        int vtx_start = drawList->VtxBuffer.Size;
        ImVec2 pMin = ImVec2(mid.x - textSize.x / 2 - 4, mid.y - textSize.y / 2 - 2);
        ImVec2 pMax = ImVec2(mid.x + textSize.x / 2 + 4, mid.y + textSize.y / 2 + 2);
        
        drawList->AddRectFilled(pMin, pMax, labelBgColor, 4.0f);
        drawList->AddRect(pMin, pMax, labelBorderColor, 4.0f);
        drawList->AddText(ImVec2(mid.x - textSize.x / 2, mid.y - textSize.y / 2), labelTextColor, label);

        int vtx_end = drawList->VtxBuffer.Size;
        float cos_a = std::cos(textAngle), sin_a = std::sin(textAngle);
        for (int i = vtx_start; i < vtx_end; i++) {
            ImVec2& p = drawList->VtxBuffer[i].pos;
            p.x -= mid.x; p.y -= mid.y; 
            float rotated_x = (p.x * cos_a) - (p.y * sin_a);
            float rotated_y = (p.x * sin_a) + (p.y * cos_a); 
            p.x = rotated_x + mid.x; p.y = rotated_y + mid.y; 
        }
    } // DrawConveyor 함수 끝

    void DrawTechnicianManager(const FactorySnap& snap) {
        if (snap.technicians.empty()) {
            return;
        }

        ImGui::SetCursorPos(ImVec2(20, 420));
        ImGui::BeginChild("TechManager", ImVec2(200, 200), true);
        ImGui::Text("Technician Manager");
        ImGui::Separator();
        
        auto it = productImages.find("TECHNICIAN");
        bool hasTechImage = (it != productImages.end() && it->second.textureID != 0);

        for (const auto& tech : snap.technicians) {
            bool isWorking = tech.targetMachineId.has_value();

            if (!isWorking && hasTechImage) {
                ImGui::Image((void*)(intptr_t)it->second.textureID, ImVec2(50, 50));
            } else if (!isWorking) {
                ImGui::Bullet(); 
            }
            
            // 이름표 생성
            std::string displayName = tech.id;
            if (tech.id == "TECH_1") displayName = "jincheol";
            else if (tech.id == "TECH_2") displayName = "jaeyong";

            // 상태 표시 텍스트
            if (isWorking) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                ImGui::Text("%s (Working)", displayName.c_str());
                ImGui::Text("  -> repairing:");
                ImGui::Text("     %s", tech.targetMachineId.value().c_str());
                ImGui::PopStyleColor();
            } else {
                ImGui::Text("%s (Idle)", displayName.c_str());
            }
            
            ImGui::Spacing();
            ImGui::Separator();
>>>>>>> e45f6f6efe8e8e0187b65db4835850ce90aa6766
        }
        ImGui::EndChild();
    }

<<<<<<< HEAD
public:
    FactoryFloorPanel() { initLayout(); }

    void render(const FactorySnap& snap, Controller* ctrl) override {
        ImGui::SetNextWindowSize(ImVec2(1400, 650), ImGuiCond_FirstUseEver);
        ImGui::Begin("Factory Floor", nullptr, ImGuiWindowFlags_AlwaysHorizontalScrollbar);

=======
    void DrawTechniciansOnFloor(const FactorySnap& snap) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 windowPos = ImGui::GetWindowPos();
        float scrollX = ImGui::GetScrollX();
        float scrollY = ImGui::GetScrollY();

        auto it = productImages.find("TECHNICIAN");
        if (it == productImages.end() || it->second.textureID == 0) return;

        GLuint texID = it->second.textureID;
        float width = 45.0f;  
        float height = 45.0f; 

        for (const auto& tech : snap.technicians) {
            if (tech.targetMachineId.has_value()) {
                std::string targetId = tech.targetMachineId.value();
                auto nodeIt = nodes_.find(targetId);
                
                if (nodeIt != nodes_.end()) {
                    ImVec2 mPos = nodeIt->second.pos;

                    float time = ImGui::GetTime();
                    float bounce = std::sin(time * 8.0f) * 5.0f; 

                    float offsetX = 125.0f; 
                    float offsetY = 15.0f;

                    ImVec2 pMin = ImVec2(
                        mPos.x + windowPos.x - scrollX + offsetX,
                        mPos.y + windowPos.y - scrollY + offsetY + bounce
                    );
                    ImVec2 pMax = ImVec2(pMin.x + width, pMin.y + height);

                    ImVec2 shadowCenter = ImVec2(
                        mPos.x + windowPos.x - scrollX + offsetX + width / 2.0f,
                        mPos.y + windowPos.y - scrollY + offsetY + height + 5.0f
                    );
                    drawList->AddEllipseFilled(shadowCenter, ImVec2(15.0f, 5.0f), IM_COL32(0, 0, 0, 80));

                    drawList->AddImage((void*)(intptr_t)texID, pMin, pMax);

                    std::string displayName = tech.id;
                    if (tech.id == "TECH_1") displayName = "jincheol";
                    else if (tech.id == "TECH_2") displayName = "jaeyong";

                    ImVec2 textSize = ImGui::CalcTextSize(displayName.c_str());
                    ImVec2 textPos = ImVec2(pMin.x + (width - textSize.x) / 2.0f, pMin.y - 15.0f);
                    
                    drawList->AddRectFilled(ImVec2(textPos.x - 2, textPos.y - 1), ImVec2(textPos.x + textSize.x + 2, textPos.y + textSize.y + 1), IM_COL32(0, 0, 0, 180), 3.0f);
                    drawList->AddText(textPos, IM_COL32(255, 255, 100, 255), displayName.c_str());
                }
            }
        }
    }

    void DrawProportionalImage(GLuint tex, float width, float targetWidth, float targetHeight) {
        // 텍스트 비율 유지 계산: targetWidth에 맞춰 비율대로 높이 산출
        float ratio = targetHeight / targetWidth;
        ImGui::Image((void*)(intptr_t)tex, ImVec2(width, width * ratio));
    }

public:
    FactoryFloorPanel() { initLayout(); }

    ~FactoryFloorPanel() {
        for (auto& pair : productImages) {
            if (pair.second.textureID != 0) {
                GLuint tex = pair.second.textureID;
                glDeleteTextures(1, &tex); 
            }
        }
        for (auto& pair : machineTextures_) {
            if (pair.second != 0) {
                glDeleteTextures(1, &pair.second);
            }
        }
    }

    void render(const FactorySnap& snap, MachineCmd& cmd) override {
        LoadProductImages();
        LoadMachineTextures();
        ImGui::BeginChild("Factory Floor", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar);
        
>>>>>>> e45f6f6efe8e8e0187b65db4835850ce90aa6766
        for (const auto& line : lines_) {
            const ConveyorSnap* foundConv = nullptr;
            for (const auto& c : snap.conveyors) {
                if (c.id == line.convId) { foundConv = &c; break; }
            }
<<<<<<< HEAD
            DrawConveyor(nodes_[line.startId].pos, nodes_[line.endId].pos, line.startAnchor, line.endAnchor, foundConv);
=======
            if (foundConv != nullptr) {
                DrawConveyor(nodes_[line.startId].pos, nodes_[line.endId].pos, line.startAnchor, line.endAnchor, foundConv);
            }
>>>>>>> e45f6f6efe8e8e0187b65db4835850ce90aa6766
        }

        for (const auto& mSnap : snap.machines) {
            auto it = nodes_.find(mSnap.id);
            if (it != nodes_.end()) {
                DrawMachineNode(mSnap, it->second);
            }
        }
<<<<<<< HEAD

        DrawTechnicianManager(snap);
        ImGui::End();
=======
        DrawTechniciansOnFloor(snap);
        DrawTechnicianManager(snap);
        ImGui::EndChild();
>>>>>>> e45f6f6efe8e8e0187b65db4835850ce90aa6766
    }
};