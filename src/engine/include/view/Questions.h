#include <Types.h>
#include <vector>

namespace mdns::engine::ui
{
    void renderQuestionLayout(std::vector<QuestionCardEntry> const& intercepted_questions);
    void renderQuestionCard(int index, std::string const &name, std::string ipAddrs);
}