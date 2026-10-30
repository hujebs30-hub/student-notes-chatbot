STUDENT NOTES CHATBOT - IMPROVED VERSION (C++ / Crow)

Features:
- Upload .txt notes or text-based .pdf notes.
- Ask questions and retrieve the best matching passage from the uploaded notes.
- Definition questions prioritize definition-style sentences. Singular/plural topic matching is supported (for example, polygon/polygons).
- Generate up to five multiple-choice fill-in-the-blank questions. Each new quiz shuffles source sentences, correct terms, and options to reduce repeated quizzes.
- Review score and explanations.

Build and run (MSYS2 UCRT64 terminal):
1. Keep your existing crow/ folder in this project directory.
2. From this project folder run:
   cmake -S . -B build -G Ninja
   cmake --build build
   ./build/chatbot.exe
3. Open http://localhost:18080 in your browser.

PDF note: PDF text extraction is performed in the browser using PDF.js loaded from a CDN, so an internet connection is required for that library. Scanned/image-only PDFs need OCR before their text can be extracted.

Important limitation: this project uses retrieval and randomized question selection, not a generative AI model. It cannot reliably understand every paraphrased question, and quizzes are varied but not guaranteed to be unique or pedagogically perfect. A real AI answer generator would require connecting an LLM API or running a local language model.
