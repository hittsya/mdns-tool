#ifndef QUESTIONS_H
#define QUESTIONS_H

#include <Types.h>
#include <chrono>
#include <vector>

namespace mdns::engine::ui {
void
renderQuestionLayout(
  std::vector<QuestionCardEntry> const& intercepted_questions);
void
renderQuestionCard(int index,
                   std::string const& name,
                   std::string ipAddrs,
                   std::chrono::steady_clock::time_point const& toa);
}

#endif // QUESTIONS_H