
#include "crow.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std;

static string notes;

static string readFile(const string& path) {
    ifstream file(path, ios::binary);
    if (!file) return "File not found: " + path;
    ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static string lowerText(string s) {
    transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) {
            return static_cast<char>(tolower(c));
        });
    return s;
}

static string normalize(const string& raw) {
    string out;

    for (unsigned char c : raw) {
        if (isalnum(c))
            out += static_cast<char>(tolower(c));
    }

    if (out.size() > 4 && out.substr(out.size() - 3) == "ies")
        out = out.substr(0, out.size() - 3) + "y";
    else if (out.size() > 3 && out.back() == 's' &&
             out.substr(out.size() - 2) != "ss")
        out.pop_back();

    return out;
}

static const unordered_set<string> stopWords = {
    "the","and","for","are","was","were","with","that","this",
    "from","have","has","had","into","its","their","there",
    "they","them","then","than","when","where","what","which",
    "who","why","how","can","could","would","should","will",
    "shall","may","might","not","but","you","your","our","out",
    "use","used","using","also","such","each","more","most",
    "some","any","all","one","two","three","four","about",
    "over","under","between","through","because","been","being",
    "does","did","do","to","of","in","on","at","by","as","an",
    "a","is","it","or","if","be","we","he","she","i","explain",
    "describe","tell","me","define","definition","give"
};

static vector<string> tokenize(const string& text) {
    istringstream in(text);
    string raw;
    vector<string> words;

    while (in >> raw) {
        string w = normalize(raw);
        if (w.size() > 2 && !stopWords.count(w))
            words.push_back(w);
    }

    return words;
}

static vector<string> splitSentences(const string& text) {
    vector<string> out;
    string cur;

    for (char c : text) {
        cur += c;

        if (c == '.' || c == '!' || c == '?' || c == '\n') {
            string s = cur;
            auto a = s.find_first_not_of(" \t\r\n");

            if (a != string::npos) {
                auto b = s.find_last_not_of(" \t\r\n");
                s = s.substr(a, b - a + 1);
                if (s.size() > 2)
                    out.push_back(s);
            }

            cur.clear();
        }
    }

    auto a = cur.find_first_not_of(" \t\r\n");

    if (a != string::npos) {
        auto b = cur.find_last_not_of(" \t\r\n");
        string s = cur.substr(a, b - a + 1);
        if (s.size() > 2)
            out.push_back(s);
    }

    return out;
}

static string trim(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos)
        return "";

    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static bool containsAny(const string& s, const vector<string>& cues) {
    for (const auto& cue : cues) {
        if (s.find(cue) != string::npos)
            return true;
    }
    return false;
}

static bool isDefinitionQuestion(const string& q) {
    string l = lowerText(q);

    return containsAny(l, {
        "what is ", "what are ", "define ",
        "definition of ", "meaning of ",
        "what do you mean by"
    });
}

static double sentenceScore(
    const string& sentence,
    const vector<string>& queryTerms,
    const unordered_map<string, int>& docFreq,
    int totalSentences,
    bool definitionQuestion
) {
    auto words = tokenize(sentence);
    if (words.empty())
        return 0.0;

    unordered_map<string, int> counts;

    for (const auto& w : words)
        counts[w]++;

    double score = 0.0;
    unordered_set<string> seen;

    for (const auto& term : queryTerms) {
        if (!seen.insert(term).second)
            continue;

        auto it = counts.find(term);

        if (it != counts.end()) {
            auto dfIt = docFreq.find(term);
            int frequency = (dfIt == docFreq.end() ? 0 : dfIt->second);

            double idf = 1.0 + log(
                1.0 + (double)(totalSentences + 1) /
                (double)(frequency + 1)
            );

            score += idf * (1.0 + log((double)it->second));
        }
    }

    string l = lowerText(sentence);

    if (definitionQuestion &&
        containsAny(l, {
            " is a ", " are ", " refers to ",
            " is defined as ", " means ",
            " can be defined as ",
            " is the process of ", " is called "
        })) {
        score *= 1.7;
    }

    if (sentence.size() > 420)
        score *= 0.85;

    return score;
}

static string answerFromNotes(const string& question) {
    auto sentences = splitSentences(notes);
    auto terms = tokenize(question);

    if (sentences.empty() || terms.empty()) {
        return "I couldn't identify the topic in your question. "
               "Please ask using the subject or key terms from your notes.";
    }

    unordered_map<string, int> df;

    for (const auto& s : sentences) {
        unordered_set<string> unique;

        for (const auto& w : tokenize(s))
            unique.insert(w);

        for (const auto& w : unique)
            df[w]++;
    }

    bool definition = isDefinitionQuestion(question);

    vector<pair<double, size_t>> ranked;

    for (size_t i = 0; i < sentences.size(); ++i) {
        double sc = sentenceScore(
            sentences[i], terms, df,
            (int)sentences.size(), definition
        );

        if (sc > 0)
            ranked.push_back({sc, i});
    }

    if (ranked.empty()) {
        return "I couldn't find this information in your uploaded notes. "
               "Try using the exact topic or a related keyword from the document.";
    }

    sort(ranked.begin(), ranked.end(),
        [](const auto& a, const auto& b) {
            return a.first > b.first;
        }
    );

    double best = ranked.front().first;

    int matched = 0;
    auto bestWords = tokenize(sentences[ranked.front().second]);
    unordered_set<string> bestSet(bestWords.begin(), bestWords.end());

    for (const auto& t : terms) {
        if (bestSet.count(t))
            matched++;
    }

    if (matched == 0 ||
        (matched == 1 && terms.size() >= 4) ||
        best < 1.4) {
        return "I couldn't find a reliable answer in your uploaded notes. "
               "Try asking with the exact topic or terminology used in the document.";
    }

    vector<size_t> chosen;
    chosen.push_back(ranked.front().second);

    for (size_t k = 1; k < ranked.size() && chosen.size() < 3; ++k) {
        if (ranked[k].first < best * 0.48)
            break;

        size_t idx = ranked[k].second;
        int shared = 0;

        auto candidateWords = tokenize(sentences[idx]);
        unordered_set<string> candidateSet(
            candidateWords.begin(), candidateWords.end()
        );

        for (const auto& t : terms) {
            if (candidateSet.count(t))
                shared++;
        }

        if (shared > 0 &&
            find(chosen.begin(), chosen.end(), idx) == chosen.end()) {
            chosen.push_back(idx);
        }
    }

    sort(chosen.begin(), chosen.end());

    string answer;

    for (size_t idx : chosen) {
        if (!answer.empty())
            answer += " ";

        answer += trim(sentences[idx]);
    }

    if (definition)
        answer = "Definition: " + answer;

    return answer;
}

int main() {
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")([] {
        return crow::response(
            200, "text/html; charset=utf-8",
            readFile("public/index.html")
        );
    });

    CROW_ROUTE(app, "/style.css")([] {
        return crow::response(
            200, "text/css; charset=utf-8",
            readFile("public/style.css")
        );
    });

    CROW_ROUTE(app, "/app.js")([] {
        return crow::response(
            200, "application/javascript; charset=utf-8",
            readFile("public/app.js")
        );
    });

    CROW_ROUTE(app, "/notes").methods(crow::HTTPMethod::POST)(
        [](const crow::request& req) {
            auto body = crow::json::load(req.body);

            if (!body || !body.has("text"))
                return crow::response(400, "Invalid notes data.");

            notes = body["text"].s();

            if (notes.empty())
                return crow::response(
                    400,
                    "No readable text was found. Scanned PDFs need OCR first."
                );

            crow::json::wvalue r;
            r["message"] = "Notes loaded successfully.";
            r["characters"] = static_cast<int>(notes.size());
            r["sentences"] =
                static_cast<int>(splitSentences(notes).size());

            return crow::response(r);
        }
    );

    CROW_ROUTE(app, "/ask").methods(crow::HTTPMethod::POST)(
        [](const crow::request& req) {
            if (notes.empty())
                return crow::response(
                    400, "Upload your PDF or TXT notes first."
                );

            auto body = crow::json::load(req.body);

            if (!body || !body.has("question"))
                return crow::response(400, "Please enter a question.");

            string question = trim(body["question"].s());

            if (question.empty())
                return crow::response(400, "Please enter a question.");

            crow::json::wvalue result;
            result["answer"] = answerFromNotes(question);

            return crow::response(result);
        }
    );

    CROW_ROUTE(app, "/quiz")([] {
        if (notes.empty())
            return crow::response(400, "Upload notes first.");

        auto sentences = splitSentences(notes);
        vector<string> usable;

        for (const auto& s : sentences) {
            if (tokenize(s).size() >= 5)
                usable.push_back(s);
        }

        random_device rd;
        mt19937 rng(rd());
        shuffle(usable.begin(), usable.end(), rng);

        unordered_set<string> usedAnswers;

        crow::json::wvalue result;
        crow::json::wvalue::list questions;

        vector<string> allWords = tokenize(notes);
        shuffle(allWords.begin(), allWords.end(), rng);

        for (const auto& sentence : usable) {
            auto words = tokenize(sentence);
            vector<string> candidates;

            for (const auto& w : words) {
                if (w.size() >= 4 &&
                    !usedAnswers.count(w) &&
                    find(candidates.begin(), candidates.end(), w)
                        == candidates.end()) {
                    candidates.push_back(w);
                }
            }

            if (candidates.empty())
                continue;

            shuffle(candidates.begin(), candidates.end(), rng);

            string normalizedAnswer = candidates.front();
            string answer;

            istringstream scan(sentence);
            string token;

            while (scan >> token) {
                if (normalize(token) == normalizedAnswer) {
                    answer = token;
                    break;
                }
            }

            if (answer.empty())
                continue;

            string prompt = sentence;
            string low = lowerText(prompt);
            string answerLow = lowerText(answer);

            size_t pos = low.find(answerLow);

            if (pos == string::npos)
                continue;

            prompt.replace(pos, answer.size(), "________");

            vector<string> options{answer};
            vector<string> distractors = allWords;

            shuffle(distractors.begin(), distractors.end(), rng);

            for (const auto& w : distractors) {
                if (options.size() == 4)
                    break;

                if (w != normalizedAnswer &&
                    find(options.begin(), options.end(), w)
                        == options.end() &&
                    abs((int)w.size() -
                        (int)normalizedAnswer.size()) <= 8) {
                    options.push_back(w);
                }
            }

            if (options.size() < 4) {
                for (const auto& w : distractors) {
                    if (options.size() == 4)
                        break;

                    if (w != normalizedAnswer &&
                        find(options.begin(), options.end(), w)
                            == options.end()) {
                        options.push_back(w);
                    }
                }
            }

            if (options.size() < 4)
                continue;

            shuffle(options.begin(), options.end(), rng);

            crow::json::wvalue item;
            item["question"] = prompt;
            item["answer"] = answer;
            item["explanation"] = sentence;

            crow::json::wvalue::list opts;

            for (const auto& option : options)
                opts.push_back(option);

            item["options"] = std::move(opts);
            questions.push_back(std::move(item));

            usedAnswers.insert(normalizedAnswer);

            if (questions.size() >= 5)
                break;
        }

        result["questions"] = std::move(questions);

        return crow::response(result);
    });

    // Render deployment configuration
    const char* portEnv = std::getenv("PORT");
    int port = portEnv ? std::stoi(portEnv) : 18080;

    app.bindaddr("0.0.0.0")
       .port(port)
       .multithreaded()
       .run();

    return 0;
}
