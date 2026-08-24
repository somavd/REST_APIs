let questions = [];

document.addEventListener("DOMContentLoaded", async () => {
  await requireAuth();
  loadQuestions();
});

async function loadQuestions() {
  try {
    const res = await fetch(`${API_BASE}/api/questions`, { credentials: "same-origin" });
    const data = await res.json();
    questions = data.questions || [];
  } catch (e) {
    questions = [];
    showError("Failed to load questions from server.");
  }
  renderQuestions();
}

function renderQuestions() {
  const table = document.getElementById("questionTable");
  table.innerHTML = "";

  questions.forEach((q, index) => {
    const row = document.createElement("tr");
    row.appendChild(textCell(index + 1));
    row.appendChild(textCell(q.title));
    row.appendChild(textCell(q.difficulty));
    row.appendChild(textCell(q.category));

    const actions = document.createElement("td");

    const editBtn = document.createElement("button");
    editBtn.textContent = "Edit";
    editBtn.addEventListener("click", () => {
      location.href = `/public/add-question.html?id=${q.id}`;
    });

    const viewBtn = document.createElement("button");
    viewBtn.textContent = "View";
    viewBtn.addEventListener("click", () => {
      location.href = `/public/question-detail.html?id=${q.id}`;
    });

    const deleteBtn = document.createElement("button");
    deleteBtn.textContent = "Delete";
    deleteBtn.addEventListener("click", () => deleteQuestion(q.id));

    actions.appendChild(editBtn);
    actions.appendChild(viewBtn);
    actions.appendChild(deleteBtn);
    row.appendChild(actions);

    table.appendChild(row);
  });
}

async function deleteQuestion(id) {
  if (!confirm("Delete this question?")) return;
  try {
    const res = await fetch(`${API_BASE}/api/questions/${id}`, {
      credentials: "same-origin",
      method: "DELETE",
    });
    const data = await res.json();
    if (!data.success) {
      showError("Failed to delete question");
      return;
    }
  } catch (e) {
    showError("Server error while deleting question.");
    return;
  }
  await loadQuestions();
}
