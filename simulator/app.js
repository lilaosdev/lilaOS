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
let serialPort = null;
let serialReader = null;
let keepReading = false;

const pageId = document.querySelector("#page-id");
const title = document.querySelector("#title");
const text = document.querySelector("#text");
const leftOption = document.querySelector("#left-option");
const rightOption = document.querySelector("#right-option");
const physicalButton = document.querySelector("#physical-button");
const connectSerial = document.querySelector("#connect-serial");
const disconnectSerial = document.querySelector("#disconnect-serial");
const reset = document.querySelector("#reset");
const connectionStatus = document.querySelector("#connection-status");

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

function renderFromDevice(state) {
  pageId.textContent = `pagina ${state.pageIndex + 1}/${state.pageCount}`;
  title.textContent = state.title;
  text.textContent = state.text;
  leftOption.textContent = state.options[0];
  rightOption.textContent = state.options[1];
  leftOption.classList.toggle("is-selected", state.selectedOption === 0);
  rightOption.classList.toggle("is-selected", state.selectedOption === 1);
}

function handleSerialLine(line) {
  if (!line.startsWith("VISUAL:")) {
    return;
  }

  try {
    renderFromDevice(JSON.parse(line.slice("VISUAL:".length)));
  } catch (error) {
    console.warn("Estado visual invalido recebido do ESP32:", error);
  }
}

async function readSerial(port) {
  const decoder = new TextDecoder();
  let buffer = "";
  keepReading = true;

  while (port.readable && keepReading) {
    serialReader = port.readable.getReader();

    try {
      while (true) {
        const { value, done } = await serialReader.read();

        if (done || !keepReading) {
          break;
        }

        buffer += decoder.decode(value, { stream: true });
        const lines = buffer.split(/\r?\n/);
        buffer = lines.pop() ?? "";
        lines.forEach(handleSerialLine);
      }
    } finally {
      serialReader.releaseLock();
      serialReader = null;
    }
  }
}

function explainSerialOpenError(error) {
  const message = error?.message ?? "";

  if (message.includes("Failed to open serial port")) {
    return "Nao consegui abrir a COM. Feche Serial Monitor, Arduino IDE, PlatformIO Monitor e qualquer outra aba/app usando o ESP32; depois desconecte e conecte o USB.";
  }

  if (message.includes("No port selected")) {
    return "Nenhuma porta selecionada.";
  }

  if (message.includes("The port is already open")) {
    return "Essa porta ja esta aberta nesta pagina. Use Desconectar antes de tentar novamente.";
  }

  return `Falha ao conectar: ${message}`;
}

async function connectToDevice() {
  if (!("serial" in navigator)) {
    connectionStatus.textContent = "Web Serial indisponivel. Use Chrome/Edge em localhost.";
    return;
  }

  try {
    serialPort = await navigator.serial.requestPort();
    await serialPort.open({ baudRate: 115200 });
    connectionStatus.textContent = "ESP32 conectado. Use o botao fisico na protoboard.";
    connectSerial.disabled = true;
    disconnectSerial.disabled = false;
    physicalButton.disabled = true;
    reset.disabled = true;
    await readSerial(serialPort);
  } catch (error) {
    connectionStatus.textContent = explainSerialOpenError(error);
    connectSerial.disabled = false;
    disconnectSerial.disabled = true;
    physicalButton.disabled = false;
    reset.disabled = false;
  }
}

async function disconnectFromDevice() {
  keepReading = false;

  if (serialReader) {
    await serialReader.cancel();
  }

  if (serialPort) {
    await serialPort.close();
  }

  serialPort = null;
  connectSerial.disabled = false;
  disconnectSerial.disabled = true;
  physicalButton.disabled = false;
  reset.disabled = false;
  connectionStatus.textContent = "Desconectado. Modo simulador local ativo.";
}

physicalButton.addEventListener("click", moveCursor);
connectSerial.addEventListener("click", connectToDevice);
disconnectSerial.addEventListener("click", disconnectFromDevice);
reset.addEventListener("click", () => {
  currentPageIndex = 0;
  selectedOption = 0;
  render();
});

render();
