#include "crow.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

using namespace std;
static string notes;
static string readFile(const string& path) {
    ifstream file(path, ios::binary);
    if (!file) return "File not found: " + path;
    ostringstream buffer; buffer << file.rdbuf(); return buffer.str();
}
static string lowerText(string s) {
    transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(tolower(c)); });
    return s;
}
static string normalize(string w) {
    string out;
    for (unsigned char c : w) if (isalnum(c)) out += static_cast<char>(tolower(c));
    // Basic English plural normalization helps "polygons" match "polygon".
    if (out.size() > 4 && out.substr(out.size()-3) == "ies") out = out.substr(0, out.size()-3) + "y";
    else if (out.size() > 3 && out.back() == 's' && out.substr(out.size()-2) != "ss") out.pop_back();
    return out;
}
static const unordered_set<string> stopWords = {
 "the","and","for","are","was","were","with","that","this","from","have","has","had","into","its","their","there","they","them","then","than","when","where","what","which","who","why","how","can","could","would","should","will","shall","may","might","not","but","you","your","our","out","use","used","using","also","such","each","more","most","some","any","all","one","two","three","four","about","over","under","between","through","because","been","being","does","did","do","to","of","in","on","at","by","as","an","a","is","it","or","if","be","we","he","she","i","explain","describe","tell","me"
};
static vector<string> tokenize(const string& text) {
    istringstream in(text); string raw; vector<string> words;
    while (in >> raw) { string w=normalize(raw); if (w.size()>2 && !stopWords.count(w)) words.push_back(w); }
    return words;
}
static vector<string> splitSentences(const string& text) {
    vector<string> out; string cur;
    for (char c : text) {
        cur += c;
        if (c=='.' || c=='!' || c=='?' || c=='\n') {
            auto a=cur.find_first_not_of(" \t\r\n");
            if(a!=string::npos){ auto b=cur.find_last_not_of(" \t\r\n"); string s=cur.substr(a,b-a+1); if(s.size()>2) out.push_back(s); }
            cur.clear();
        }
    }
    auto a=cur.find_first_not_of(" \t\r\n");
    if(a!=string::npos){ auto b=cur.find_last_not_of(" \t\r\n"); string s=cur.substr(a,b-a+1); if(s.size()>2) out.push_back(s); }
    return out;
}
static bool hasAny(const string& s, const vector<string>& cues) { for (const auto& c:cues) if(s.find(c)!=string::npos) return true; return false; }
static string trim(const string& s) {
    size_t a=s.find_first_not_of(" \t\r\n"); if(a==string::npos) return "";
    size_t b=s.find_last_not_of(" \t\r\n"); return s.substr(a,b-a+1);
}
int main() {
    crow::SimpleApp app;
    CROW_ROUTE(app, "/")([] { return crow::response(200,"text/html; charset=utf-8",readFile("public/index.html")); });
    CROW_ROUTE(app, "/style.css")([] { return crow::response(200,"text/css; charset=utf-8",readFile("public/style.css")); });
    CROW_ROUTE(app, "/app.js")([] { return crow::response(200,"application/javascript; charset=utf-8",readFile("public/app.js")); });

    CROW_ROUTE(app,"/notes").methods(crow::HTTPMethod::POST)([](const crow::request& req){
        auto body=crow::json::load(req.body);
        if(!body || !body.has("text")) return crow::response(400,"Invalid notes data.");
        notes=body["text"].s();
        if(notes.empty()) return crow::response(400,"No readable text was found. Scanned PDFs need OCR first.");
        crow::json::wvalue r; r["message"]="Notes loaded successfully."; r["characters"]=static_cast<int>(notes.size()); r["sentences"]=static_cast<int>(splitSentences(notes).size());
        return crow::response(r);
    });

    CROW_ROUTE(app,"/ask").methods(crow::HTTPMethod::POST)([](const crow::request& req){
        auto body=crow::json::load(req.body);
        if(!body || !body.has("question")) return crow::response(400,"Please enter a question.");
        if(notes.empty()) return crow::response(400,"Upload notes first.");
        string question=body["question"].s();
        auto qwords=tokenize(question); auto sentences=splitSentences(notes);
        if(qwords.empty()) return crow::response(400,"Please ask a question about a topic in your notes.");
        string q=lowerText(question);
        bool isDefinition=hasAny(q,{"what is","what are","define","definition of","meaning of"});
        vector<pair<double,size_t>> ranked;
        for(size_t i=0;i<sentences.size();++i){
            string s=lowerText(sentences[i]); auto words=tokenize(sentences[i]); unordered_set<string> unique(words.begin(),words.end());
            int matched=0; double score=0;
            for(const auto& w:qwords){ if(unique.count(w)){matched++; score+=5.0;} }
            if(!matched) continue;
            double coverage=static_cast<double>(matched)/qwords.size(); score+=coverage*8.0;
            // Definition phrases get priority only for definition questions.
            bool defCue=hasAny(s,{" is a "," is an "," are "," refers to "," defined as "," means "," is the "," can be defined as "});
            bool procedure=hasAny(s,{"step 1","step 2","click","select","follow these","algorithm","procedure","first,","next,"});
            if(isDefinition && defCue) score+=12;
            if(isDefinition && procedure && !defCue) score-=9;
            if(sentences[i].size()<30) score-=2;
            if(sentences[i].size()>500) score-=2;
            ranked.push_back({score,i});
        }
        sort(ranked.begin(),ranked.end(),[](auto a,auto b){return a.first>b.first;});
        crow::json::wvalue result;
        if(ranked.empty() || ranked.front().first < 8.0){
            result["answer"]="I couldn't identify a reliable answer in the uploaded notes. Try asking with the exact topic name or wording used in your PDF.";
            result["source"]="";
        } else {
            size_t idx=ranked.front().second; string answer=trim(sentences[idx]);
            // Include the immediately following sentence only if it discusses the same terms.
            if(!isDefinition && idx+1<sentences.size()){
                auto nextWords=tokenize(sentences[idx+1]); unordered_set<string> next(nextWords.begin(),nextWords.end()); int overlap=0;
                for(const auto& w:qwords) if(next.count(w)) overlap++;
                if(overlap>0 && answer.size()+sentences[idx+1].size()<650) answer += " " + trim(sentences[idx+1]);
            }
            result["answer"]=answer; result["source"]=answer;
        }
        result["method"]="Retrieved from the most relevant matching passage in your uploaded notes. This is note-based retrieval, not a generative AI model.";
        return crow::response(result);
    });

    CROW_ROUTE(app,"/quiz")([]{
        if(notes.empty()) return crow::response(400,"Upload notes first.");
        auto sentences=splitSentences(notes);
        vector<string> usable;
        for(const auto& s:sentences) if(tokenize(s).size()>=5) usable.push_back(s);
        random_device rd; mt19937 rng(rd());
        shuffle(usable.begin(),usable.end(),rng); // New source order for every quiz request.
        unordered_set<string> usedAnswers;
        crow::json::wvalue result; crow::json::wvalue::list questions;
        vector<string> allWords=tokenize(notes); shuffle(allWords.begin(),allWords.end(),rng);
        for(const auto& sentence:usable){
            auto words=tokenize(sentence); vector<string> candidates;
            for(const auto& w:words) if(w.size()>=4 && !usedAnswers.count(w) && find(candidates.begin(),candidates.end(),w)==candidates.end()) candidates.push_back(w);
            if(candidates.empty()) continue;
            shuffle(candidates.begin(),candidates.end(),rng);
            string answer=candidates.front();
            string prompt=sentence; string low=lowerText(prompt); size_t pos=low.find(answer);
            // If capitalization or plural form differs, locate the raw word by normalized token scan.
            if(pos==string::npos){
                istringstream scan(prompt); string token; size_t offset=0;
                while(scan>>token){ size_t p=prompt.find(token,offset); if(normalize(token)==answer){pos=p; answer=token; break;} offset=p+token.size(); }
            }
            if(pos==string::npos) continue;
            prompt.replace(pos,answer.size(),"________");
            vector<string> options{answer};
            vector<string> distractors=allWords; shuffle(distractors.begin(),distractors.end(),rng);
            for(const auto& w:distractors){
                if(options.size()==4) break;
                if(w!=normalize(answer) && find(options.begin(),options.end(),w)==options.end() && abs((int)w.size()-(int)answer.size())<=8) options.push_back(w);
            }
            if(options.size()<4) for(const auto& w:distractors){ if(options.size()==4) break; if(w!=normalize(answer) && find(options.begin(),options.end(),w)==options.end()) options.push_back(w); }
            if(options.size()<4) continue;
            shuffle(options.begin(),options.end(),rng);
            crow::json::wvalue item; item["question"]=prompt; item["answer"]=answer; item["explanation"]=sentence;
            crow::json::wvalue::list opts; for(const auto& option:options) opts.push_back(option); item["options"]=std::move(opts);
            questions.push_back(std::move(item)); usedAnswers.insert(normalize(answer));
            if(questions.size()>=5) break;
        }
        result["questions"]=std::move(questions);
        return crow::response(result);
    });
    app.port(18080).multithreaded().run();
}
