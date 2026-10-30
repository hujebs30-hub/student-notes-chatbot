STUDENT NOTES CHATBOT

Features
- Upload text-based PDF or TXT notes.
- Ask questions about the uploaded notes. The backend ranks relevant sentences and prioritizes definition-style sentences for questions such as "What is a polygon?".
- Generate a multiple-choice quiz from the uploaded notes.
- Check answers and review the score and explanations.

Important answer-quality note
This version uses local, extractive information retrieval in C++ (term matching with inverse-document-frequency weighting and definition cues). It returns relevant text from the uploaded notes and avoids inventing an answer when it cannot find a reasonable match. It is NOT a generative AI model and cannot guarantee semantic correctness for every paraphrased or complex question. Check important answers against the PDF. For genuinely conversational, paraphrase-aware answers, integrate a language model with retrieval-augmented generation (RAG).

PDF note
PDF text is extracted in the browser using PDF.js. Scanned/image-only PDFs need OCR before upload.

Build and run (MSYS2 UCRT64)
1. Keep the Crow source folder named "crow" in this project directory.
2. Open MSYS2 UCRT64 and change to this folder, for example:
   cd /c/odd_26/student-notes-chatbot
3. Configure:
   cmake -S . -B build -G Ninja
4. Build:
   cmake --build build
5. Run:
   ./build/chatbot.exe
6. Open http://localhost:18080

The Crow folder is a dependency and is not included in this ZIP. Keep the crow folder from your previous setup.
