import { createServer } from "node:http";
import { readFile } from "node:fs/promises";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = dirname(dirname(fileURLToPath(import.meta.url)));
const port = Number(process.env.PORT ?? 8787);

const server = createServer(async (request, response) => {
  if (request.url !== "/config") {
    response.writeHead(404, { "content-type": "application/json" });
    response.end(JSON.stringify({ error: "not_found" }));
    return;
  }

  try {
    const config = await readFile(join(root, "config.json"), "utf8");
    response.writeHead(200, {
      "cache-control": "no-store",
      "content-type": "application/json",
    });
    response.end(config);
  } catch {
    response.writeHead(500, { "content-type": "application/json" });
    response.end(JSON.stringify({ error: "config_unavailable" }));
  }
});

server.listen(port, () => {
  console.log(`Config server listening on http://localhost:${port}/config`);
});

