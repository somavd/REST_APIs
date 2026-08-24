let questionId = null;
let testcases = [];

document.addEventListener("DOMContentLoaded", async () => {
  await requireAuth();
  const params = new URLSearchParams(location.search);
  const id = params.get("id");
  if (id) {
    questionId = parseInt(id, 10);
    if (isNaN(questionId)) {
      showError("Invalid question ID");
      return;
    }
    document.getElementById("pageTitle").textContent = "Edit Question";
    document.getElementById("saveBtn").textContent = "Update Question";
    document.getElementById("testCaseSection").style.display = "block";
    await loadQuestion(questionId);
  } else {
    document.getElementById("pageTitle").textContent = "Add Question";
    document.getElementById("saveBtn").textContent = "Save Question";
    document.getElementById("testCaseSection").style.display = "none";
  }
});

async function loadQuestion(id) {
  try {
    const res = await fetch(`${API_BASE}/api/questions`, { credentials: "same-origin" });
    if (!res.ok) throw new Error("Failed to load questions");
    const data = await res.json();
    const q = (data.questions || []).find((item) => item.id === id);
    if (!q) throw new Error("Question not found");
    document.getElementById("title").value = q.title || "";
    document.getElementById("description").value = q.description || "";
    document.getElementById("category").value = q.category || "";
    document.getElementById("difficulty").value = q.difficulty || "Easy";
    document.getElementById("published").checked = !!q.published;
    await loadTestCases(id);
  } catch (e) {
    showError("Failed to load question.");
  }
}

document.getElementById("questionForm").addEventListener("submit", async (e) => {
  e.preventDefault();
  const payload = {
    title: document.getElementById("title").value.trim(),
    description: document.getElementById("description").value.trim(),
    category: document.getElementById("category").value.trim(),
    difficulty: document.getElementById("difficulty").value,
    published: document.getElementById("published").checked,
  };

  if (!payload.title) {
    showError("Title is required");
    return;
  }

  try {
    let res;
    if (questionId) {
      res = await fetch(`${API_BASE}/api/questions/${questionId}`, {
        method: "PUT",
        headers: { "Content-Type": "application/json" },
        credentials: "same-origin",
        body: JSON.stringify(payload),
      });
    } else {
      res = await fetch(`${API_BASE}/api/questions`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        credentials: "same-origin",
        body: JSON.stringify(payload),
      });
    }

    const data = await res.json();
    if (!res.ok || !data.success) {
      showError(data.error || "Failed to save question");
      return;
    }

    if (!questionId && data.id) {
      questionId = data.id;
      document.getElementById("testCaseSection").style.display = "block";
      document.getElementById("saveBtn").textContent = "Update Question";
      const url = new URL(location.href);
      url.searchParams.set("id", questionId);
      history.replaceState(null, "", url.toString());
    }

    showStatus("Question saved.", false);
  } catch (e) {
    showError("Server error while saving question.");
  }
});

async function loadTestCases(id) {
  try {
    const res = await fetch(`${API_BASE}/api/questions/${id}/testcases`, {
      credentials: "same-origin",
    });
    if (!res.ok) throw new Error("Failed to load test cases");
    const data = await res.json();
    testcases = data.testcases || [];
  } catch (e) {
    testcases = [];
    showError("Failed to load test cases.");
  }
  renderTestCases();
}

async function addTestCase() {
  if (!questionId) {
    showError("Save the question before adding test cases");
    return;
  }
  const input = document.getElementById("testInput").value;
  const expected_output = document.getElementById("testExpected").value;
  const is_hidden = document.getElementById("testHidden").checked;

  if (!input || !expected_output) {
    showError("Input and expected output are required");
    return;
  }

  try {
    const res = await fetch(`${API_BASE}/api/questions/${questionId}/testcases`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      credentials: "same-origin",
      body: JSON.stringify({ input, expected_output, is_hidden }),
    });
    const data = await res.json();
    if (!res.ok || !data.success) {
      showError(data.error || "Failed to add test case");
      return;
    }
    document.getElementById("testInput").value = "";
    document.getElementById("testExpected").value = "";
    document.getElementById("testHidden").checked = false;
    await loadTestCases(questionId);
  } catch (e) {
    showError("Server error while adding test case.");
  }
}

async function deleteTestCase(id) {
  if (!confirm("Delete this test case?")) return;
  try {
    const res = await fetch(`${API_BASE}/api/testcases/${id}`, {
      credentials: "same-origin",
      method: "DELETE",
    });
    const data = await res.json();
    if (!data.success) {
      showError("Failed to delete test case");
      return;
    }
  } catch (e) {
    showError("Server error while deleting test case.");
    return;
  }
  await loadTestCases(questionId);
}

function renderTestCases() {
  const table = document.getElementById("testcaseTable");
  table.innerHTML = "";
  if (testcases.length === 0) {
    const row = document.createElement("tr");
    const cell = document.createElement("td");
    cell.colSpan = 5;
    cell.textContent = "No test cases yet.";
    row.appendChild(cell);
    table.appendChild(row);
    return;
  }
  testcases.forEach((t, index) => {
    const row = document.createElement("tr");
    row.appendChild(textCell(index + 1));
    row.appendChild(textCell(t.input));
    row.appendChild(textCell(t.expected_output));
    row.appendChild(textCell(t.is_hidden ? "Hidden" : "Public"));
    const actions = document.createElement("td");
    const del = document.createElement("button");
    del.textContent = "Delete";
    del.addEventListener("click", () => deleteTestCase(t.id));
    actions.appendChild(del);
    row.appendChild(actions);
    table.appendChild(row);
  });
}
