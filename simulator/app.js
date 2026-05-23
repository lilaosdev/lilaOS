const pages = [
  {
    title: "Oi",
    text: "Preparei uma coisinha pequena, mas feita com todo cuidado.",
    options: ["Continuar", "Respirar"],
  },
  {
    title: "Prometo ser breve",
    text: "So preciso de mais alguns cliques antes da pergunta importante.",
    options: ["Anterior", "Proxima"],
  },
];

let currentPageIndex = 0;
let selectedOption = 0;

const pageId = document.querySelector("#page-id");
const title = document.querySelector("#title");
const text = document.querySelector("#text");
const leftOption = document.querySelector("#left-option");
const rightOption = document.querySelector("#right-option");
const physicalButton = document.querySelector("#physical-button");
const reset = document.querySelector("#reset");

function moveCursor() {
  selectedOption++;

  if (selectedOption > 1) {
    selectedOption = 0;
    currentPageIndex = (currentPageIndex + 1) % pages.length;
  }

  render();
}

function render() {
  const page = pages[currentPageIndex];

  pageId.textContent = `pagina ${currentPageIndex + 1}/${pages.length}`;
  title.textContent = page.title;
  text.textContent = page.text;
  leftOption.textContent = page.options[0];
  rightOption.textContent = page.options[1];
  leftOption.classList.toggle("is-selected", selectedOption === 0);
  rightOption.classList.toggle("is-selected", selectedOption === 1);
}

physicalButton.addEventListener("click", moveCursor);
reset.addEventListener("click", () => {
  currentPageIndex = 0;
  selectedOption = 0;
  render();
});

render();
