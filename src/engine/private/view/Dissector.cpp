#include <view/Dissector.h>
#include <style/Window.h>
#include <imgui.h>

void mdns::engine::ui::renderDissectorWindow(std::optional<ScanCardEntry> const& dissector_meta_entry, bool* show)
{
    auto const card = dissector_meta_entry.value_or(mdns::engine::ScanCardEntry{"Unknown"});

    ImGuiViewport *vp = ImGui::GetMainViewport();
    ImVec2 const size = {
        vp->WorkSize.x * 0.5f,
        vp->WorkSize.y * 0.5f,
    };

    ImGui::SetNextWindowSize(size, ImGuiCond_Always);

    mdns::engine::ui::pushThemedWindowStyles();
    ImGui::Begin(("Dissected mDNS RR's: " + card.name).c_str(), show, ImGuiWindowFlags_None);
    mdns::engine::ui::popThemedWindowStyles();

    ImGui::BeginGroup();
    ImGui::Dummy(ImVec2(0.0f, 0.0f));

    auto const textColor = ImVec4(0.26f, 0.59f, 0.98f, 1.0f);

    for (auto const &rr: card.dissector_meta) {
        std::visit([&]<typename T0>(T0 const &entry) {
            using T = std::decay_t<T0>;

            ImGui::PushID(&rr);

            if constexpr(std::is_same_v<T, proto::mdns_rr_ptr_ext>) {
                ImGui::TextColored(textColor, "PTR record");
                ImGui::Indent();
                ImGui::Text("Target: %s", entry.target.c_str());
                ImGui::Unindent();
            }
            else if constexpr(std::is_same_v<T, proto::mdns_rr_txt_ext>) {
                ImGui::TextColored(textColor, "TXT record");
                ImGui::Indent();
                
                if (entry.entries.empty()) {
                    ImGui::Text("0-bytes TXT record");
                } else {
                    for (auto const& txt : entry.entries) {
                        ImGui::Text("%s", txt.c_str());
                    }
                }

                ImGui::Unindent();
            }
            else if constexpr(std::is_same_v<T, proto::mdns_rr_srv_ext>) {
                ImGui::TextColored(textColor, "SRV record");
                ImGui::Indent();
                ImGui::Text("Target:   %s", entry.target.c_str());
                ImGui::Text("Port:     %u", entry.port);
                ImGui::Text("Priority: %u", entry.priority);
                ImGui::Text("Weight:   %u", entry.weight);
                ImGui::Unindent();
            }
            else if constexpr (std::is_same_v<T, proto::mdns_rr_a_ext>) {
                ImGui::TextColored(textColor, "A record");
                ImGui::Indent();
                ImGui::Text("IpV4:     %s", entry.address.c_str());
                ImGui::Unindent();
            }
            else if constexpr(std::is_same_v<T, proto::mdns_rr_aaaa_ext>) {
                ImGui::TextColored(textColor, "AAAA record");
                ImGui::Indent();
                ImGui::Text("IpV6:     %s", entry.address.c_str());
                ImGui::Unindent();
            }
            else if constexpr(std::is_same_v<T, proto::mdns_rr_nsec_ext>) {
                ImGui::TextColored(textColor, "NSEC record");
                ImGui::Indent();

                ImGui::Text("Next domain: %s", entry.next_domain.c_str());
                if (!entry.types.empty()) {
                    ImGui::Text("Types:");
                    ImGui::Indent();

                    for (auto t : entry.types) {
                        const char* name = "UNKNOWN";

                        switch (t) {
                            case proto::MDNS_RECORDTYPE_A:    name = "A"   ; break;
                            case proto::MDNS_RECORDTYPE_AAAA: name = "AAAA"; break;
                            case proto::MDNS_RECORDTYPE_PTR:  name = "PTR" ; break;
                            case proto::MDNS_RECORDTYPE_TXT:  name = "TXT" ; break;
                            case proto::MDNS_RECORDTYPE_SRV:  name = "SRV" ; break;
                            case proto::MDNS_RECORDTYPE_NSEC: name = "NSEC"; break;
                        }

                        ImGui::Text("%s (%u)", name, t);
                    }

                    ImGui::Unindent();
                } else {
                    ImGui::TextDisabled("No type bitmap present");
                }

                ImGui::Unindent();
            }
            else {
                ImGui::TextColored(textColor, "UNKNOWN record");
                ImGui::Indent();

                const auto& data = entry.raw;

                if (data.empty()) {
                    ImGui::TextDisabled("<empty>");
                } else {
                    constexpr int bytes_per_row = 16;

                    for (size_t i = 0; i < data.size(); i += bytes_per_row) {
                        std::string hex;
                        std::string ascii;

                        for (size_t j = 0; j < bytes_per_row && i + j < data.size(); ++j) {
                            uint8_t b = data[i + j];

                            char buf[4];
                            std::snprintf(buf, sizeof(buf), "%02X ", b);
                            hex += buf;

                            ascii += (b >= 32 && b <= 126) ? static_cast<char>(b) : '.';
                        }

                        ImGui::Text("%04zx  %-48s  %s", i, hex.c_str(), ascii.c_str());
                    }
                }

                ImGui::Unindent();
            }

            ImGui::PopID();
            ImGui::Spacing(); }, rr);
    }

    ImGui::EndGroup();
    ImGui::End();
}
