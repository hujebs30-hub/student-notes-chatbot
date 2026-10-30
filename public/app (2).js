const fileInput = document.getElementById("notesFile");
const uploadBtn = document.getElementById("uploadBtn");
const uploadStatus = document.getElementById("uploadStatus");
const askForm = document.getElementById("askForm");
const questionInput = document.getElementById("question");
const answerBox = document.getElementById("answer");
const quizBtn = document.getElementById("quizBtn");
const quizForm = document.getElementById("quizForm");
const quizQuestions = document.getElementById("quizQuestions");
const quizResult = document.getElementById("quizResult");
let currentQuiz = [];

async function api(url, options = {}) {
  const response = await fetch(url, options);
  const text = await response.text();
  let data;
  try { data = JSON.parse(text); } catch { data = { message: text }; }
  if (!response.ok) throw new Error(data.message || text || "Request failed.");
  return data;
}

async function extractPdfText(file) {
  if (!window.pdfjsLib) throw new Error("PDF reader did not load. Check your internet connection, refresh, and try again.");
  pdfjsLib.GlobalWorkerOptions.workerSrc = "https://cdnjs.cloudflare.com/ajax/libs/pdf.js/3.11.174/pdf.worker.min.js";
  const bytes = new Uint8Array(await file.arrayBuffer());
  const pdf = await pdfjsLib.getDocument({ data: bytes }).promise;
  const pages = [];
  for (let pageNumber = 1; pageNumber <= pdf.numPages; pageNumber++) {
    uploadStatus.textContent = `Reading PDF page ${pageNumber} of ${pdf.numPages}...`;
    const page = await pdf.getPage(pageNumber);
    const content = await page.getTextContent();
    pages.push(content.items.map(item => item.str).join(" "));
  }
  return pages.join("\n\n").trim();
}

uploadBtn.addEventListener("click", async () => {
  const file = fileInput.files[0];
  if (!file) { uploadStatus.textContent = "Choose a .txt or .pdf file first."; return; }
  const extension = file.name.split(".").pop().toLowerCase();
  if (!(["txt", "pdf"].includes(extension))) { uploadStatus.textContent = "Only .txt and .pdf files are supported."; return; }
  try {
    uploadBtn.disabled = true;
    uploadStatus.textContent = "Reading your notes...";
    let text;
    if (extension === "pdf") text = await extractPdfText(file);
    else text = await file.text();
    if (!text.trim()) throw new Error("No text could be extracted. If this is a scanned PDF, use OCR to convert it to selectable text first.");
    const data = await api("/notes", {
      method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify({ text })
    });
    uploadStatus.textContent = `${data.message} (${data.characters} characters, ${data.sentences} text sections)`;
    answerBox.textContent = "Notes are ready. Ask a question above.";
    quizResult.classList.add("hidden");
    quizForm.classList.add("hidden");
  } catch (error) { uploadStatus.textContent = error.message; }
  finally { uploadBtn.disabled = false; }
});

askForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  const question = questionInput.value.trim();
  if (!question) return;
  answerBox.textContent = "Searching for the most relevant parts of your notes...";
  try {
    const data = await api("/ask", {
      method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify({ question })
    });
    answerBox.replaceChildren();
    const answer = document.createElement("p");
    answer.textContent = data.answer;
    answerBox.append(answer);
    if (data.source) {
      const source = document.createElement("p");
      source.className = "source-note";
      source.textContent = `From your notes: “${data.source}”`;
      answerBox.append(source);
    }
    const method = document.createElement("small");
    method.className = "method-note";
    method.textContent = data.method || "";
    answerBox.append(method);
  } catch (error) { answerBox.textContent = error.message; }
});

quizBtn.addEventListener("click", async () => {
  quizResult.classList.add("hidden"); quizForm.classList.add("hidden"); quizQuestions.replaceChildren();
  try {
    quizBtn.disabled = true;
    const data = await api("/quiz"); currentQuiz = data.questions || [];
    if (!currentQuiz.length) {
      quizResult.textContent = "Not enough distinct terms to create a multiple-choice quiz. Upload more detailed notes.";
      quizResult.classList.remove("hidden"); return;
    }
    currentQuiz.forEach((item, index) => {
      const wrapper = document.createElement("fieldset"); wrapper.className = "quiz-question";
      const prompt = document.createElement("legend"); prompt.textContent = `Q${index + 1}. ${item.question}`;
      wrapper.append(prompt);
      item.options.forEach((option, optionIndex) => {
        const label = document.createElement("label"); label.className = "option-label";
        const input = document.createElement("input"); input.type = "radio"; input.name = `quiz-answer-${index}`; input.value = option; input.required = true;
        label.append(input, document.createTextNode(` ${String.fromCharCode(65 + optionIndex)}. ${option}`));
        wrapper.append(label);
      });
      quizQuestions.append(wrapper);
    });
    quizForm.classList.remove("hidden");
  } catch (error) { quizResult.textContent = error.message; quizResult.classList.remove("hidden"); }
  finally { quizBtn.disabled = false; }
});

quizForm.addEventListener("submit", (event) => {
  event.preventDefault(); let score = 0; quizResult.replaceChildren();
  const review = document.createElement("div");
  currentQuiz.forEach((item, index) => {
    const selected = quizForm.querySelector(`input[name="quiz-answer-${index}"]:checked`);
    const correct = selected && selected.value.toLowerCase() === item.answer.toLowerCase();
    if (correct) score++;
    const line = document.createElement("p");
    line.className = correct ? "review-correct" : "review-wrong";
    line.textContent = `Q${index + 1}: ${correct ? "Correct" : `Incorrect — correct answer: ${item.answer}`}. ${item.explanation}`;
    review.append(line);
  });
  const total = currentQuiz.length;
  const heading = document.createElement("h3"); heading.textContent = `Your score: ${score}/${total} (${Math.round(score / total * 100)}%)`;
  const tip = document.createElement("p");
  if (score === total) tip.textContent = "Excellent. Try explaining each answer without looking at your notes.";
  else if (score >= Math.ceil(total / 2)) tip.textContent = "Good effort. Review the explanations for the questions you missed, then retry.";
  else tip.textContent = "Review the source sentences in your notes, then generate the quiz again for another attempt.";
  quizResult.append(heading, tip, review); quizResult.classList.remove("hidden");
});
