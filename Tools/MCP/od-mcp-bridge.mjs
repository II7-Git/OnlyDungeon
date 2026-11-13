#!/usr/bin/env node
// OnlyDungeon MCP 브리지 (stdio <-> Streamable HTTP, 세션 없는 MCP 2026-07-28 방식)
//
// 용도: Claude 데스크톱 앱(stdio 전용)을 UE 5.9 에디터 내장 MCP 서버(HTTP)에 연결한다.
//
// 배경 (Claude Code CLI 트래픽 기록으로 확인한 UE 5.9 서버 동작):
//   - 서버는 세션 없는 MCP 프로토콜 2026-07-28 로 정상 동작한다.
//     initialize 없이 server/discover 로 정보를 얻고, 모든 요청에
//     params._meta["io.modelcontextprotocol/protocolVersion" | "clientInfo" | "clientCapabilities"]
//     와 mcp-method / mcp-protocol-version 헤더를 붙인다.
//   - 구버전(2025-11-25) initialize 세션 방식은 initialize 이후 요청이 거부된다.
//   - Claude 데스크톱 앱은 구버전 방식(initialize -> notifications/initialized -> 요청)으로 말한다.
//   => 이 브리지는 앱의 initialize 를 server/discover 로 바꿔 응답을 합성하고,
//      이후 요청은 2026-07-28 형식으로 변환해 서버에 보낸다.
//
// 사용법: node od-mcp-bridge.mjs http://127.0.0.1:8000/mcp [--log <파일경로>]
// 의존성: 없음 (Node 18+ 내장 fetch 사용)
//
// 주의: stdout 은 MCP 메시지 전용이다. 로그는 반드시 stderr 또는 로그 파일로만 출력한다.

import { createInterface } from "node:readline";
import { appendFileSync } from "node:fs";

const args = process.argv.slice(2);
const SERVER_URL = args.find((a) => /^https?:\/\//i.test(a)) || "http://127.0.0.1:8000/mcp";
const logIdx = args.indexOf("--log");
const LOG_FILE = logIdx >= 0 ? args[logIdx + 1] : null;

// 서버와 통신할 프로토콜 버전 (UE 5.9 서버가 정상 처리하는 세션 없는 방식)
const WIRE_VERSION = "2026-07-28";
const NS = "io.modelcontextprotocol/";

function log(...parts) {
  const line = `[${new Date().toISOString()}] ${parts
    .map((p) => (typeof p === "string" ? p : JSON.stringify(p)))
    .join(" ")}`;
  process.stderr.write(line + "\n");
  if (LOG_FILE) {
    try {
      appendFileSync(LOG_FILE, line + "\n");
    } catch {
      /* 로그 실패는 무시 */
    }
  }
}

// 앱이 initialize 때 알려 준 클라이언트 정보 (모든 요청 _meta 에 실어 보낸다)
let clientInfo = { name: "claude-desktop-bridge", version: "1.0.0" };
let clientCapabilities = {};

function send(msg) {
  process.stdout.write(JSON.stringify(msg) + "\n");
}

function sendError(id, code, message) {
  if (id === undefined || id === null) return;
  send({ jsonrpc: "2.0", id, error: { code, message } });
}

// HTTP 헤더에는 ASCII 만 들어갈 수 있으므로 그 외 문자는 퍼센트 인코딩한다.
function encodeHeader(v) {
  return /^[\x20-\x7E]*$/.test(v) ? v : encodeURIComponent(v);
}

function buildHeaders(method, params) {
  const h = {
    "Content-Type": "application/json",
    Accept: "application/json, text/event-stream",
    "mcp-method": method,
    "mcp-protocol-version": WIRE_VERSION,
  };
  // 이름 기반 호출은 대상 이름을 mcp-name 헤더로 함께 보낸다.
  const p = params || {};
  if ((method === "tools/call" || method === "prompts/get") && typeof p.name === "string") h["mcp-name"] = encodeHeader(p.name);
  else if (method === "resources/read" && typeof p.uri === "string") h["mcp-name"] = encodeHeader(p.uri);
  return h;
}

// 요청 params 에 2026-07-28 방식의 _meta 를 채운다.
function withMeta(params) {
  const out = params && typeof params === "object" ? { ...params } : {};
  const meta = out._meta && typeof out._meta === "object" ? { ...out._meta } : {};
  meta[NS + "protocolVersion"] = WIRE_VERSION;
  meta[NS + "clientInfo"] = clientInfo;
  meta[NS + "clientCapabilities"] = clientCapabilities;
  out._meta = meta;
  return out;
}

// SSE 본문에서 JSON-RPC 메시지들을 꺼낸다.
async function readSse(body) {
  const reader = body.getReader();
  const decoder = new TextDecoder();
  const out = [];
  let buf = "";
  let dataLines = [];
  const flush = () => {
    if (dataLines.length === 0) return;
    const data = dataLines.join("\n");
    dataLines = [];
    try {
      const parsed = JSON.parse(data);
      out.push(...(Array.isArray(parsed) ? parsed : [parsed]));
    } catch {
      log("JSON 이 아닌 SSE 데이터 무시", data.slice(0, 200));
    }
  };
  for (;;) {
    const { value, done } = await reader.read();
    if (done) break;
    buf += decoder.decode(value, { stream: true });
    let nl;
    while ((nl = buf.indexOf("\n")) >= 0) {
      let line = buf.slice(0, nl);
      buf = buf.slice(nl + 1);
      if (line.endsWith("\r")) line = line.slice(0, -1);
      if (line === "") flush();
      else if (line.startsWith("data:")) dataLines.push(line.slice(5).replace(/^ /, ""));
    }
  }
  flush();
  return out;
}

// 서버에 한 건 POST 하고, 돌아온 JSON-RPC 메시지 배열을 반환한다.
async function post(message) {
  const res = await fetch(SERVER_URL, {
    method: "POST",
    headers: buildHeaders(message.method, message.params),
    body: JSON.stringify(message),
  });
  const ctype = (res.headers.get("content-type") || "").toLowerCase();
  let msgs = [];
  if (ctype.includes("text/event-stream") && res.body) {
    msgs = await readSse(res.body);
  } else {
    const text = await res.text();
    if (text.trim()) {
      try {
        const parsed = JSON.parse(text);
        msgs = Array.isArray(parsed) ? parsed : [parsed];
      } catch {
        log(`JSON 이 아닌 응답 (HTTP ${res.status})`, text.slice(0, 300));
      }
    }
  }
  if (!res.ok) log(`HTTP ${res.status} (${message.method})`, msgs[0] && msgs[0].error ? msgs[0].error : "");
  return { status: res.status, msgs };
}

// 앱의 initialize 에 대한 응답을 server/discover 결과로 합성한다.
async function handleInitialize(msg) {
  const p = msg.params || {};
  if (p.clientInfo) clientInfo = p.clientInfo;
  if (p.capabilities) clientCapabilities = p.capabilities;
  const requested = p.protocolVersion || "2025-06-18";

  try {
    const { msgs } = await post({
      jsonrpc: "2.0",
      id: "od-bridge-discover",
      method: "server/discover",
      params: withMeta({}),
    });
    const r = msgs.find((m) => m && m.id === "od-bridge-discover");
    if (!r || r.error || !r.result) {
      const err = (r && r.error) || { code: -32603, message: "server/discover 응답 없음" };
      log("server/discover 실패", err);
      send({ jsonrpc: "2.0", id: msg.id, error: err });
      return;
    }
    const result = r.result;
    // 앱에는 앱이 요청한 버전 그대로 답한다. 서버 쪽 통신 버전 변환은 브리지가 맡는다.
    const clientVersion = requested;
    const serverInfo = (result._meta && result._meta[NS + "serverInfo"]) || { name: "unreal-mcp", version: "unknown" };
    send({
      jsonrpc: "2.0",
      id: msg.id,
      result: {
        protocolVersion: clientVersion,
        capabilities: result.capabilities || { tools: {} },
        serverInfo,
        instructions: result.instructions,
      },
    });
    log(`연결 완료: ${serverInfo.name} ${serverInfo.version} (앱 ${clientVersion} <-> 서버 ${WIRE_VERSION})`);
  } catch (e) {
    log("서버 연결 실패", String(e));
    sendError(msg.id, -32603, `UE MCP 서버(${SERVER_URL})에 연결할 수 없음. 에디터가 실행 중인지 확인: ${e}`);
  }
}

// 일반 요청/알림을 2026-07-28 형식으로 변환해 전달한다.
async function handleMessage(msg) {
  // 응답(앱 -> 서버 방향의 result/error)은 세션 없는 방식에서 보낼 곳이 없으므로 버린다.
  if (typeof msg.method !== "string") return;

  // 구버전 세션 전용 알림/요청은 서버로 보내지 않는다.
  if (msg.method === "notifications/initialized") return;
  if (msg.method === "ping") {
    if (msg.id !== undefined) send({ jsonrpc: "2.0", id: msg.id, result: {} });
    return;
  }

  const out = { ...msg, params: withMeta(msg.params) };
  try {
    const { status, msgs } = await post(out);
    const isRequest = msg.id !== undefined && msg.id !== null;
    let answered = false;
    for (const m of msgs) {
      if (!m) continue;
      if (m.error && (m.id === undefined || m.id === null)) {
        // id 없는 에러 -> 요청이었다면 그 요청의 에러로 돌려준다.
        if (isRequest && !answered) {
          send({ jsonrpc: "2.0", id: msg.id, error: m.error });
          answered = true;
        }
        continue;
      }
      if (m.error) log(`서버 에러 (${msg.method})`, m.error);
      if (m.id === msg.id) answered = true;
      send(m);
    }
    if (isRequest && !answered) sendError(msg.id, -32603, `서버 응답 없음 (HTTP ${status})`);
  } catch (e) {
    log("서버 연결 실패", String(e));
    if (msg.id !== undefined) sendError(msg.id, -32603, `UE MCP 서버(${SERVER_URL})에 연결할 수 없음. 에디터가 실행 중인지 확인: ${e}`);
  }
}

// initialize 가 끝나기 전에 다른 요청이 앞질러 가지 않도록 한다.
let ready = Promise.resolve();
const rl = createInterface({ input: process.stdin, crlfDelay: Infinity });
rl.on("line", (line) => {
  if (!line.trim()) return;
  let parsed;
  try {
    parsed = JSON.parse(line);
  } catch {
    log("stdin 에서 JSON 이 아닌 입력 무시", line.slice(0, 200));
    return;
  }
  for (const m of Array.isArray(parsed) ? parsed : [parsed]) {
    if (m && m.method === "initialize") ready = ready.then(() => handleInitialize(m));
    else ready.then(() => handleMessage(m));
  }
});
rl.on("close", () => {
  ready.finally(() => setTimeout(() => process.exit(0), 300));
});

log(`브리지 시작: ${SERVER_URL} (서버 통신 버전 ${WIRE_VERSION})`);
