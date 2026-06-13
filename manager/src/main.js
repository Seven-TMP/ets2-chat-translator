const { app, BrowserWindow, dialog, ipcMain } = require('electron');
const fs = require('fs');
const path = require('path');
const childProcess = require('child_process');
const crypto = require('crypto');

const DLL_NAME = 'ets2_chat_translator.dll';
const CONFIG_NAME = 'ets2_chat_translator_config.json';
const PRESETS_NAME = 'config_presets.json';

function iconPath() {
  const packaged = path.join(process.resourcesPath || '', 'logo.ico');
  if (packaged && fs.existsSync(packaged)) return packaged;
  return path.resolve(__dirname, '..', '..', 'logo.ico');
}

const GAMES = {
  ets2: {
    shortName: 'ETS2',
    name: 'Euro Truck Simulator 2',
    exe: 'eurotrucks2.exe',
    steamAppId: '227300',
    steamFolderName: 'Euro Truck Simulator 2',
    registryKeys: [
      'SOFTWARE\\SCS Software\\Euro Truck Simulator 2',
      'SOFTWARE\\WOW6432Node\\SCS Software\\Euro Truck Simulator 2'
    ],
    guesses: [
      'C:\\Program Files (x86)\\Steam\\steamapps\\common\\Euro Truck Simulator 2',
      'D:\\Steam\\steamapps\\common\\Euro Truck Simulator 2',
      'D:\\SteamLibrary\\steamapps\\common\\Euro Truck Simulator 2',
      'E:\\SteamLibrary\\steamapps\\common\\Euro Truck Simulator 2'
    ]
  },
  ats: {
    shortName: 'ATS',
    name: 'American Truck Simulator',
    exe: 'amtrucks.exe',
    steamAppId: '270880',
    steamFolderName: 'American Truck Simulator',
    registryKeys: [
      'SOFTWARE\\SCS Software\\American Truck Simulator',
      'SOFTWARE\\WOW6432Node\\SCS Software\\American Truck Simulator'
    ],
    guesses: [
      'C:\\Program Files (x86)\\Steam\\steamapps\\common\\American Truck Simulator',
      'D:\\Steam\\steamapps\\common\\American Truck Simulator',
      'D:\\SteamLibrary\\steamapps\\common\\American Truck Simulator',
      'E:\\SteamLibrary\\steamapps\\common\\American Truck Simulator'
    ]
  }
};

function gameDef(game) {
  return GAMES[game] || GAMES.ets2;
}

let mainWindow;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1320,
    height: 860,
    minWidth: 880,
    minHeight: 640,
    title: 'ETS2 Chat Translator Manager',
    backgroundColor: '#f4f5f7',
    autoHideMenuBar: true,
    icon: iconPath(),
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false
    }
  });

  mainWindow.loadFile(path.join(__dirname, 'index.html'));
}

app.whenReady().then(createWindow);

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});

function appRoot() {
  if (app.isPackaged) return path.dirname(process.execPath);
  return path.resolve(__dirname, '..', '..');
}

function sourceDllPath() {
  const packaged = path.join(process.resourcesPath || '', DLL_NAME);
  if (packaged && fs.existsSync(packaged)) return packaged;

  const besideExe = path.join(appRoot(), DLL_NAME);
  if (fs.existsSync(besideExe)) return besideExe;

  return path.join(appRoot(), 'build', DLL_NAME);
}

function pluginDir(ets2Path) {
  return path.join(ets2Path, 'bin', 'win_x64', 'plugins');
}

function installedDllPath(ets2Path) {
  return path.join(pluginDir(ets2Path), DLL_NAME);
}

function configPath(ets2Path) {
  return path.join(pluginDir(ets2Path), CONFIG_NAME);
}

function presetsPath() {
  return path.join(app.getPath('userData'), PRESETS_NAME);
}

function readPresets() {
  const file = presetsPath();
  if (!fs.existsSync(file)) return [];
  try {
    const parsed = JSON.parse(fs.readFileSync(file, 'utf8'));
    return Array.isArray(parsed.presets) ? parsed.presets.filter((item) => item && item.name && item.config) : [];
  } catch {
    return [];
  }
}

function writePresets(presets) {
  const file = presetsPath();
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, JSON.stringify({ presets }, null, 2), 'utf8');
  return presets;
}

function normalizePresetName(name) {
  return String(name || '').replace(/\s+/g, ' ').trim().slice(0, 48);
}

function savePreset(name, jsonText) {
  const cleanName = normalizePresetName(name);
  if (!cleanName) throw new Error('预设名称不能为空');
  const config = JSON.parse(jsonText);
  const presets = readPresets();
  const next = {
    name: cleanName,
    updated_at: new Date().toISOString(),
    config
  };
  const index = presets.findIndex((item) => item.name.toLowerCase() === cleanName.toLowerCase());
  if (index >= 0) presets[index] = next;
  else presets.push(next);
  presets.sort((a, b) => a.name.localeCompare(b.name, 'zh-CN'));
  return writePresets(presets);
}

function deletePreset(name) {
  const cleanName = normalizePresetName(name);
  if (!cleanName) return readPresets();
  return writePresets(readPresets().filter((item) => item.name.toLowerCase() !== cleanName.toLowerCase()));
}

function compactText(value, max = 260) {
  return String(value || '').replace(/\s+/g, ' ').trim().slice(0, max);
}

function redactSecrets(text, provider) {
  let out = String(text || '');
  for (const secret of [provider?.api_key, provider?.api_secret]) {
    if (secret && secret.length >= 6) out = out.split(secret).join('[已隐藏]');
  }
  return out;
}

function joinProviderUrl(baseUrl, fallback, pathWithQuery) {
  const u = new URL(baseUrl || fallback);
  const prefix = u.pathname.replace(/\/+$/, '');
  return `${u.protocol}//${u.host}${prefix}${pathWithQuery}`;
}

function jsonString(body, keys) {
  try {
    const value = JSON.parse(body || '{}');
    const stack = [value];
    while (stack.length) {
      const item = stack.shift();
      if (!item || typeof item !== 'object') continue;
      for (const key of keys) {
        if (typeof item[key] === 'string' && item[key]) return item[key];
      }
      if (Array.isArray(item)) stack.push(...item);
      else stack.push(...Object.values(item));
    }
  } catch {
    return '';
  }
  return '';
}

function errorFromReply(reply, provider) {
  if (reply.error) return reply.error;
  const parsed = jsonString(reply.body, [
    'message',
    'error',
    'error_msg',
    'errorMessage',
    'ErrorMessage',
    'Code',
    'code',
    'type'
  ]);
  return redactSecrets(parsed || compactText(reply.body), provider);
}

function requestText(method, urlText, headers = {}, body = '', timeoutMs = 5000) {
  return new Promise((resolve) => {
    const started = Date.now();
    let url;
    try {
      url = new URL(urlText);
    } catch (error) {
      resolve({ status: 0, body: '', error: `bad url: ${error.message}`, elapsedMs: 0 });
      return;
    }

    const lib = url.protocol === 'http:' ? require('http') : require('https');
    const data = body ? Buffer.from(body, 'utf8') : null;
    const req = lib.request(url, {
      method,
      headers: {
        ...headers,
        ...(data ? { 'Content-Length': data.length } : {})
      },
      timeout: Math.max(1500, Math.min(6000, timeoutMs || 5000))
    }, (res) => {
      let responseBody = '';
      res.setEncoding('utf8');
      res.on('data', (chunk) => {
        if (responseBody.length < 5000) responseBody += chunk;
      });
      res.on('end', () => {
        resolve({
          status: res.statusCode || 0,
          body: responseBody,
          elapsedMs: Date.now() - started
        });
      });
    });
    req.on('timeout', () => req.destroy(new Error('timeout')));
    req.on('error', (error) => {
      resolve({ status: 0, body: '', error: error.message, elapsedMs: Date.now() - started });
    });
    if (data) req.write(data);
    req.end();
  });
}

function hmacHex(algorithm, key, value) {
  return crypto.createHmac(algorithm, key).update(value).digest('hex');
}

function hmacBase64(algorithm, key, value) {
  return crypto.createHmac(algorithm, key).update(value).digest('base64');
}

function sha256Hex(value) {
  return crypto.createHash('sha256').update(value).digest('hex');
}

function md5Hex(value) {
  return crypto.createHash('md5').update(value).digest('hex');
}

function rfc3986(value) {
  return encodeURIComponent(String(value))
    .replace(/[!'()*]/g, (ch) => `%${ch.charCodeAt(0).toString(16).toUpperCase()}`);
}

function sortedQuery(params) {
  return Object.entries(params)
    .sort(([a], [b]) => a.localeCompare(b))
    .map(([key, value]) => `${rfc3986(key)}=${rfc3986(value)}`)
    .join('&');
}

function dateUtc() {
  return new Date().toISOString().slice(0, 10);
}

function iso8601Utc() {
  return new Date().toISOString().replace(/\.\d{3}Z$/, 'Z');
}

function compactIsoUtc() {
  return new Date().toISOString().replace(/[-:]/g, '').replace(/\.\d{3}Z$/, 'Z');
}

function targetSimple(target) {
  const value = String(target || 'zh-CN');
  if (value === 'zh-CN' || value === 'zh-Hans') return 'zh';
  return value;
}

function targetMicrosoft(target) {
  const value = String(target || 'zh-CN');
  if (value === 'zh' || value === 'zh-CN') return 'zh-Hans';
  return value;
}

function targetBaidu(target) {
  const value = String(target || 'zh-CN');
  if (value === 'zh-CN' || value === 'zh-Hans') return 'zh';
  return value;
}

function targetYoudao(target) {
  const value = String(target || 'zh-CN');
  if (value === 'zh-CN' || value === 'zh' || value === 'zh-Hans') return 'zh-CHS';
  return value;
}

function targetDeepL(target) {
  const value = String(target || 'zh-CN').toUpperCase();
  if (value === 'ZH-CN' || value === 'ZH-HANS' || value === 'ZH') return 'ZH';
  return value;
}

function sourceOrAuto(provider) {
  return provider.source || provider.source_lang || 'auto';
}

function providerName(provider) {
  return provider.label || provider.kind || 'Provider';
}

function missing(provider, fields) {
  const names = fields.filter((field) => !provider[field]);
  return names.length ? `缺少 ${names.join(', ')}` : '';
}

async function finishProviderTest(provider, runtime, request) {
  const reply = await request();
  const ok = reply.status >= 200 && reply.status < 300;
  return {
    label: providerName(provider),
    kind: provider.kind || '',
    ok,
    status: reply.status,
    elapsed_ms: reply.elapsedMs,
    error: ok ? '' : errorFromReply(reply, provider),
    sample: runtime.sampleText
  };
}

async function testAnthropic(provider, runtime) {
  const miss = missing(provider, ['api_key', 'model']);
  if (miss) throw new Error(miss);
  const body = JSON.stringify({
    model: provider.model,
    max_tokens: 96,
    messages: [{ role: 'user', content: `Translate to ${runtime.target}: ${runtime.sampleText}` }]
  });
  return finishProviderTest(provider, runtime, () => requestText('POST',
    joinProviderUrl(provider.base_url, 'https://api.anthropic.com/v1', '/messages'),
    {
      'Content-Type': 'application/json',
      Accept: 'application/json',
      'x-api-key': provider.api_key,
      'anthropic-version': '2023-06-01'
    },
    body,
    runtime.timeoutMs));
}

async function testOpenAI(provider, runtime) {
  const miss = missing(provider, ['model']);
  if (miss) throw new Error(miss);
  const body = JSON.stringify({
    model: provider.model,
    temperature: 0,
    messages: [
      { role: 'system', content: `Translate chat into ${runtime.target}. Output only the translation.` },
      { role: 'user', content: runtime.sampleText }
    ]
  });
  const headers = {
    'Content-Type': 'application/json',
    Accept: 'application/json'
  };
  if (provider.api_key) headers.Authorization = `Bearer ${provider.api_key}`;
  return finishProviderTest(provider, runtime, () => requestText('POST',
    joinProviderUrl(provider.base_url, 'https://api.openai.com/v1', '/chat/completions'),
    headers,
    body,
    runtime.timeoutMs));
}

async function testMyMemory(provider, runtime) {
  const source = sourceOrAuto(provider) === 'auto' ? 'en' : sourceOrAuto(provider);
  const url = `https://api.mymemory.translated.net/get?q=${encodeURIComponent(runtime.sampleText)}&langpair=${encodeURIComponent(`${source}|${runtime.target}`)}`;
  return finishProviderTest(provider, runtime, () => requestText('GET', url, { Accept: 'application/json' }, '', runtime.timeoutMs));
}

async function testAndeer(provider, runtime) {
  const url = `https://api.andeer.top/API/fanyi2.php?msg=${encodeURIComponent(runtime.sampleText)}`;
  return finishProviderTest(provider, runtime, () => requestText('GET', url, { Accept: 'application/json' }, '', runtime.timeoutMs));
}

async function testDeepL(provider, runtime) {
  const miss = missing(provider, ['api_key']);
  if (miss) throw new Error(miss);
  const form = new URLSearchParams();
  form.set('text', runtime.sampleText);
  form.set('target_lang', targetDeepL(runtime.target));
  if (sourceOrAuto(provider) !== 'auto') form.set('source_lang', sourceOrAuto(provider).toUpperCase());
  return finishProviderTest(provider, runtime, () => requestText('POST',
    joinProviderUrl(provider.base_url, 'https://api-free.deepl.com', '/v2/translate'),
    {
      'Content-Type': 'application/x-www-form-urlencoded',
      Accept: 'application/json',
      Authorization: `DeepL-Auth-Key ${provider.api_key}`
    },
    form.toString(),
    runtime.timeoutMs));
}

async function testGoogle(provider, runtime) {
  const miss = missing(provider, ['api_key']);
  if (miss) throw new Error(miss);
  const params = new URLSearchParams({
    key: provider.api_key,
    q: runtime.sampleText,
    target: targetSimple(runtime.target),
    format: 'text'
  });
  if (sourceOrAuto(provider) !== 'auto') params.set('source', sourceOrAuto(provider));
  return finishProviderTest(provider, runtime, () => requestText('GET',
    joinProviderUrl(provider.base_url, 'https://translation.googleapis.com', `/language/translate/v2?${params}`),
    { Accept: 'application/json' },
    '',
    runtime.timeoutMs));
}

async function testLibre(provider, runtime) {
  const body = {
    q: runtime.sampleText,
    source: sourceOrAuto(provider),
    target: targetSimple(runtime.target),
    format: 'text'
  };
  if (provider.api_key) body.api_key = provider.api_key;
  return finishProviderTest(provider, runtime, () => requestText('POST',
    joinProviderUrl(provider.base_url, 'https://libretranslate.com', '/translate'),
    {
      'Content-Type': 'application/json',
      Accept: 'application/json'
    },
    JSON.stringify(body),
    runtime.timeoutMs));
}

async function testMicrosoft(provider, runtime) {
  const miss = missing(provider, ['api_key']);
  if (miss) throw new Error(miss);
  const params = new URLSearchParams({ 'api-version': '3.0', to: targetMicrosoft(runtime.target) });
  if (sourceOrAuto(provider) !== 'auto') params.set('from', sourceOrAuto(provider));
  const headers = {
    'Content-Type': 'application/json',
    Accept: 'application/json',
    'Ocp-Apim-Subscription-Key': provider.api_key
  };
  if (provider.api_secret) headers['Ocp-Apim-Subscription-Region'] = provider.api_secret;
  return finishProviderTest(provider, runtime, () => requestText('POST',
    joinProviderUrl(provider.base_url, 'https://api.cognitive.microsofttranslator.com', `/translate?${params}`),
    headers,
    JSON.stringify([{ Text: runtime.sampleText }]),
    runtime.timeoutMs));
}

async function testBaidu(provider, runtime) {
  const miss = missing(provider, ['api_key', 'api_secret']);
  if (miss) throw new Error(miss);
  const salt = String(Date.now());
  const params = new URLSearchParams({
    q: runtime.sampleText,
    from: sourceOrAuto(provider),
    to: targetBaidu(runtime.target),
    appid: provider.api_key,
    salt,
    sign: md5Hex(`${provider.api_key}${runtime.sampleText}${salt}${provider.api_secret}`)
  });
  return finishProviderTest(provider, runtime, () => requestText('GET',
    joinProviderUrl(provider.base_url, 'https://fanyi-api.baidu.com', `/api/trans/vip/translate?${params}`),
    { Accept: 'application/json' },
    '',
    runtime.timeoutMs));
}

async function testYoudao(provider, runtime) {
  const miss = missing(provider, ['api_key', 'api_secret']);
  if (miss) throw new Error(miss);
  const salt = String(Date.now());
  const curtime = Math.floor(Date.now() / 1000).toString();
  const q = runtime.sampleText;
  const input = q.length <= 20 ? q : `${q.slice(0, 10)}${q.length}${q.slice(-10)}`;
  const form = new URLSearchParams({
    q,
    from: sourceOrAuto(provider),
    to: targetYoudao(runtime.target),
    appKey: provider.api_key,
    salt,
    sign: sha256Hex(`${provider.api_key}${input}${salt}${curtime}${provider.api_secret}`),
    signType: 'v3',
    curtime
  });
  return finishProviderTest(provider, runtime, () => requestText('POST',
    joinProviderUrl(provider.base_url, 'https://openapi.youdao.com', '/api'),
    {
      'Content-Type': 'application/x-www-form-urlencoded',
      Accept: 'application/json'
    },
    form.toString(),
    runtime.timeoutMs));
}

async function testTencent(provider, runtime) {
  const miss = missing(provider, ['api_key', 'api_secret']);
  if (miss) throw new Error(miss);
  const base = new URL(provider.base_url || 'https://tmt.tencentcloudapi.com');
  const timestamp = Math.floor(Date.now() / 1000).toString();
  const date = dateUtc();
  const service = 'tmt';
  const region = provider.model || 'ap-guangzhou';
  const body = JSON.stringify({
    SourceText: runtime.sampleText,
    Source: sourceOrAuto(provider) === 'auto' ? 'auto' : sourceOrAuto(provider),
    Target: targetSimple(runtime.target),
    ProjectId: 0
  });
  const canonicalHeaders = `content-type:application/json\nhost:${base.host}\n`;
  const signedHeaders = 'content-type;host';
  const credentialScope = `${date}/${service}/tc3_request`;
  const canonicalRequest = `POST\n/\n\n${canonicalHeaders}\n${signedHeaders}\n${sha256Hex(body)}`;
  const stringToSign = `TC3-HMAC-SHA256\n${timestamp}\n${credentialScope}\n${sha256Hex(canonicalRequest)}`;
  const dateKey = crypto.createHmac('sha256', `TC3${provider.api_secret}`).update(date).digest();
  const serviceKey = crypto.createHmac('sha256', dateKey).update(service).digest();
  const signingKey = crypto.createHmac('sha256', serviceKey).update('tc3_request').digest();
  const signature = crypto.createHmac('sha256', signingKey).update(stringToSign).digest('hex');
  const authorization = `TC3-HMAC-SHA256 Credential=${provider.api_key}/${credentialScope}, SignedHeaders=${signedHeaders}, Signature=${signature}`;
  return finishProviderTest(provider, runtime, () => requestText('POST',
    `${base.protocol}//${base.host}${base.pathname === '/' ? '' : base.pathname}`,
    {
      'Content-Type': 'application/json',
      Accept: 'application/json',
      Authorization: authorization,
      Host: base.host,
      'X-TC-Action': 'TextTranslate',
      'X-TC-Version': '2018-03-21',
      'X-TC-Timestamp': timestamp,
      'X-TC-Region': region
    },
    body,
    runtime.timeoutMs));
}

async function testAliyun(provider, runtime) {
  const miss = missing(provider, ['api_key', 'api_secret']);
  if (miss) throw new Error(miss);
  const scene = provider.model || 'general';
  const params = {
    AccessKeyId: provider.api_key,
    Action: 'TranslateGeneral',
    Format: 'JSON',
    FormatType: 'text',
    RegionId: 'cn-hangzhou',
    Scene: scene,
    SignatureMethod: 'HMAC-SHA1',
    SignatureNonce: String(Date.now()),
    SignatureVersion: '1.0',
    SourceLanguage: sourceOrAuto(provider) === 'auto' ? 'auto' : sourceOrAuto(provider),
    SourceText: runtime.sampleText,
    TargetLanguage: targetSimple(runtime.target),
    Timestamp: iso8601Utc(),
    Version: '2018-10-12'
  };
  const canonical = sortedQuery(params);
  const signature = hmacBase64('sha1', `${provider.api_secret}&`, `GET&%2F&${rfc3986(canonical)}`);
  return finishProviderTest(provider, runtime, () => requestText('GET',
    joinProviderUrl(provider.base_url, 'https://mt.cn-hangzhou.aliyuncs.com', `/?${canonical}&Signature=${rfc3986(signature)}`),
    { Accept: 'application/json' },
    '',
    runtime.timeoutMs));
}

async function testVolcengine(provider, runtime) {
  const miss = missing(provider, ['api_key', 'api_secret']);
  if (miss) throw new Error(miss);
  const base = new URL(provider.base_url || 'https://translate.volcengineapi.com');
  const region = provider.model || 'cn-north-1';
  const date = dateUtc().replace(/-/g, '');
  const amzDate = compactIsoUtc();
  const bodyObject = {
    TextList: [runtime.sampleText],
    TargetLanguage: targetSimple(runtime.target)
  };
  if (sourceOrAuto(provider) !== 'auto') bodyObject.SourceLanguage = sourceOrAuto(provider);
  const body = JSON.stringify(bodyObject);
  const payloadHash = sha256Hex(body);
  const canonicalHeaders = `content-type:application/json\nhost:${base.host}\nx-content-sha256:${payloadHash}\nx-date:${amzDate}\n`;
  const signedHeaders = 'content-type;host;x-content-sha256;x-date';
  const canonicalRequest = `POST\n/\nAction=TranslateText&Version=2020-06-01\n${canonicalHeaders}\n${signedHeaders}\n${payloadHash}`;
  const credentialScope = `${date}/${region}/translate/request`;
  const stringToSign = `HMAC-SHA256\n${amzDate}\n${credentialScope}\n${sha256Hex(canonicalRequest)}`;
  const dateKey = crypto.createHmac('sha256', provider.api_secret).update(date).digest();
  const regionKey = crypto.createHmac('sha256', dateKey).update(region).digest();
  const serviceKey = crypto.createHmac('sha256', regionKey).update('translate').digest();
  const signingKey = crypto.createHmac('sha256', serviceKey).update('request').digest();
  const signature = crypto.createHmac('sha256', signingKey).update(stringToSign).digest('hex');
  const authorization = `HMAC-SHA256 Credential=${provider.api_key}/${credentialScope}, SignedHeaders=${signedHeaders}, Signature=${signature}`;
  return finishProviderTest(provider, runtime, () => requestText('POST',
    `${base.protocol}//${base.host}/?Action=TranslateText&Version=2020-06-01`,
    {
      'Content-Type': 'application/json',
      Accept: 'application/json',
      Host: base.host,
      'X-Date': amzDate,
      'X-Content-Sha256': payloadHash,
      Authorization: authorization
    },
    body,
    runtime.timeoutMs));
}

async function testProvider(provider, runtime) {
  const kind = String(provider.kind || '').toLowerCase();
  try {
    if (kind === 'anthropic' || kind === 'claude' || kind === 'anthropic_messages') return await testAnthropic(provider, runtime);
    if (kind === 'openai_compatible' || kind === 'openai' || kind === 'chat_completions') return await testOpenAI(provider, runtime);
    if (kind === 'mymemory') return await testMyMemory(provider, runtime);
    if (kind === 'andeer') return await testAndeer(provider, runtime);
    if (kind === 'deepl') return await testDeepL(provider, runtime);
    if (kind === 'google_cloud' || kind === 'google_translate') return await testGoogle(provider, runtime);
    if (kind === 'libretranslate' || kind === 'libre_translate') return await testLibre(provider, runtime);
    if (kind === 'microsoft' || kind === 'azure_translator') return await testMicrosoft(provider, runtime);
    if (kind === 'baidu' || kind === 'baidu_translate') return await testBaidu(provider, runtime);
    if (kind === 'youdao') return await testYoudao(provider, runtime);
    if (kind === 'tencent' || kind === 'tencent_cloud' || kind === 'tencent_tmt') return await testTencent(provider, runtime);
    if (kind === 'aliyun' || kind === 'alibaba' || kind === 'alibaba_cloud' || kind === 'alimt') return await testAliyun(provider, runtime);
    if (kind === 'volcengine' || kind === 'volc' || kind === 'volc_translate' || kind === 'huoshan') return await testVolcengine(provider, runtime);
    throw new Error(`暂不支持测试 kind=${provider.kind || '(empty)'}`);
  } catch (error) {
    return {
      label: providerName(provider),
      kind: provider.kind || '',
      ok: false,
      status: 0,
      elapsed_ms: 0,
      error: redactSecrets(error.message, provider),
      sample: runtime.sampleText
    };
  }
}

async function testConfigText(jsonText) {
  const config = JSON.parse(jsonText);
  const providers = (config.providers || []).filter((provider) => provider && provider.enabled && provider.kind);
  const runtime = {
    target: config.target_lang || 'zh-CN',
    timeoutMs: Math.max(1500, Math.min(6000, config.timeout_ms || 5000)),
    sampleText: 'hello'
  };
  if (!providers.length) {
    return { ok: false, summary: '没有启用任何 Provider', results: [] };
  }

  const results = [];
  for (const provider of providers) {
    results.push(await testProvider(provider, runtime));
  }

  const okCount = results.filter((item) => item.ok).length;
  return {
    ok: okCount > 0,
    summary: `${okCount}/${results.length} 个 Provider 可用`,
    results
  };
}

function looksLikeGame(gamePath, game) {
  if (!gamePath || !fs.existsSync(gamePath)) return false;
  const def = gameDef(game);
  return fs.existsSync(path.join(gamePath, 'bin', 'win_x64', def.exe));
}

function uniqueExistingDirs(items) {
  const seen = new Set();
  const out = [];
  for (const item of items) {
    if (!item) continue;
    const normalized = path.normalize(String(item).replace(/^"|"$/g, '').trim());
    const key = normalized.toLowerCase();
    if (seen.has(key) || !fs.existsSync(normalized)) continue;
    seen.add(key);
    out.push(normalized);
  }
  return out;
}

function queryRegistryValue(fullKey, valueName) {
  try {
    const output = childProcess.execFileSync('reg', ['query', fullKey, '/v', valueName], {
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'ignore'],
      windowsHide: true
    });
    const line = output.split(/\r?\n/).find((item) => item.includes(valueName));
    if (!line) return '';
    const parts = line.trim().split(/\s{2,}/);
    return parts[parts.length - 1] || '';
  } catch {
    return '';
  }
}

function queryRegistryInstallDir(key) {
  return queryRegistryValue(`HKLM\\${key}`, 'InstallDir') || queryRegistryValue(`HKCU\\${key}`, 'InstallDir');
}

function queryRegistryInstallLocation(fullKey) {
  return queryRegistryValue(fullKey, 'InstallLocation') ||
    queryRegistryValue(fullKey, 'InstallDir') ||
    queryRegistryValue(fullKey, 'Path');
}

function commonDriveRoots(folderName) {
  const roots = [];
  for (let code = 67; code <= 90; ++code) {
    roots.push(`${String.fromCharCode(code)}:\\${folderName}`);
  }
  return roots;
}

function steamRootCandidates() {
  const registryRoots = [
    'HKCU\\SOFTWARE\\Valve\\Steam',
    'HKLM\\SOFTWARE\\Valve\\Steam',
    'HKLM\\SOFTWARE\\WOW6432Node\\Valve\\Steam'
  ].flatMap((key) => [
    queryRegistryValue(key, 'InstallPath'),
    queryRegistryValue(key, 'SteamPath')
  ]);

  const envRoots = [
    process.env.ProgramFiles && path.join(process.env.ProgramFiles, 'Steam'),
    process.env['ProgramFiles(x86)'] && path.join(process.env['ProgramFiles(x86)'], 'Steam'),
    process.env.ProgramW6432 && path.join(process.env.ProgramW6432, 'Steam')
  ];

  return uniqueExistingDirs([
    ...registryRoots,
    ...envRoots,
    ...commonDriveRoots('Steam')
  ]);
}

function unescapeVdfPath(value) {
  return String(value || '').replace(/\\\\/g, '\\').replace(/\\"/g, '"');
}

function vdfValue(text, key) {
  const escaped = key.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  const match = new RegExp(`"${escaped}"\\s+"([^"]*)"`, 'i').exec(text || '');
  return match ? unescapeVdfPath(match[1]) : '';
}

function steamLibraryFolders(steamRoot) {
  const folders = [steamRoot];
  const files = [
    path.join(steamRoot, 'steamapps', 'libraryfolders.vdf'),
    path.join(steamRoot, 'config', 'libraryfolders.vdf')
  ].filter((file) => fs.existsSync(file));

  for (const file of files) {
    try {
      const text = fs.readFileSync(file, 'utf8');
      for (const match of text.matchAll(/"path"\s+"([^"]+)"/gi)) {
        folders.push(unescapeVdfPath(match[1]));
      }

      // Older Steam clients stored numbered library entries as "1" "D:\\SteamLibrary".
      for (const match of text.matchAll(/"\d+"\s+"([^"]+)"/g)) {
        const value = unescapeVdfPath(match[1]);
        if (/^[A-Za-z]:\\/.test(value) || /^\\\\/.test(value)) folders.push(value);
      }
    } catch {
      // Keep scanning the other libraryfolders.vdf locations.
    }
  }

  return uniqueExistingDirs([
    ...folders,
    ...commonDriveRoots('SteamLibrary')
  ]);
}

function steamGamePathFromLibrary(libraryPath, def) {
  const steamapps = path.join(libraryPath, 'steamapps');
  const manifest = path.join(steamapps, `appmanifest_${def.steamAppId}.acf`);
  if (fs.existsSync(manifest)) {
    try {
      const text = fs.readFileSync(manifest, 'utf8');
      const installDir = vdfValue(text, 'installdir');
      if (installDir) return path.join(steamapps, 'common', installDir);
    } catch {
      // Keep the folder-name fallback below.
    }
  }
  return path.join(steamapps, 'common', def.steamFolderName);
}

function steamGameCandidates(def) {
  const libraries = uniqueExistingDirs([
    ...steamRootCandidates().flatMap(steamLibraryFolders),
    ...commonDriveRoots('SteamLibrary')
  ]);
  return libraries.map((library) => steamGamePathFromLibrary(library, def));
}

function gameRegistryCandidates(def) {
  const uninstallKeys = [
    `HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App ${def.steamAppId}`,
    `HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App ${def.steamAppId}`,
    `HKLM\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App ${def.steamAppId}`
  ];

  return [
    ...def.registryKeys.flatMap((key) => [
      queryRegistryInstallDir(key),
      queryRegistryInstallLocation(`HKLM\\${key}`),
      queryRegistryInstallLocation(`HKCU\\${key}`)
    ]),
    ...uninstallKeys.map(queryRegistryInstallLocation)
  ];
}

function detectGamePath(game) {
  const def = gameDef(game);
  const candidates = [
    ...gameRegistryCandidates(def),
    ...steamGameCandidates(def),
    ...def.guesses
  ];

  return uniqueExistingDirs(candidates).find((item) => looksLikeGame(item, game)) || '';
}

function installState(game, ets2Path) {
  const def = gameDef(game);
  if (!looksLikeGame(ets2Path, game)) {
    return {
      ok: false,
      text: `${def.shortName} 路径未设置或不像有效安装目录`,
      pluginDir: ''
    };
  }

  const dllInstalled = fs.existsSync(installedDllPath(ets2Path));
  const configExists = fs.existsSync(configPath(ets2Path));
  return {
    ok: true,
    dllInstalled,
    configExists,
    pluginDir: pluginDir(ets2Path),
    text: `${def.shortName}: ${dllInstalled ? 'DLL 已安装' : 'DLL 未安装'}，${configExists ? '配置存在' : '配置不存在'}`
  };
}

ipcMain.handle('detect-path', (_event, game) => detectGamePath(game));

ipcMain.handle('browse-path', async (_event, game) => {
  const def = gameDef(game);
  const result = await dialog.showOpenDialog(mainWindow, {
    title: `选择 ${def.name} 安装目录`,
    properties: ['openDirectory']
  });
  return result.canceled ? '' : result.filePaths[0];
});

ipcMain.handle('state', (_event, game, ets2Path) => installState(game, ets2Path));

ipcMain.handle('install-dll', (_event, game, ets2Path) => {
  const def = gameDef(game);
  if (!looksLikeGame(ets2Path, game)) throw new Error(`请先选择有效的 ${def.shortName} 安装目录`);
  const src = sourceDllPath();
  if (!fs.existsSync(src)) throw new Error(`未找到 ${DLL_NAME}，请先运行 build.bat`);
  fs.mkdirSync(pluginDir(ets2Path), { recursive: true });
  fs.copyFileSync(src, installedDllPath(ets2Path));
  return installState(game, ets2Path);
});

ipcMain.handle('uninstall-dll', (_event, game, ets2Path) => {
  const dll = installedDllPath(ets2Path);
  if (fs.existsSync(dll)) fs.unlinkSync(dll);
  return installState(game, ets2Path);
});

ipcMain.handle('read-config', (_event, _game, ets2Path) => {
  const file = configPath(ets2Path);
  if (!fs.existsSync(file)) return null;
  return fs.readFileSync(file, 'utf8');
});

ipcMain.handle('write-config', (_event, game, ets2Path, jsonText) => {
  const def = gameDef(game);
  if (!looksLikeGame(ets2Path, game)) throw new Error(`请先选择有效的 ${def.shortName} 安装目录`);
  const file = configPath(ets2Path);
  fs.mkdirSync(path.dirname(file), { recursive: true });
  JSON.parse(jsonText);
  fs.writeFileSync(file, jsonText, 'utf8');
  return installState(game, ets2Path);
});

ipcMain.handle('test-config', (_event, jsonText) => testConfigText(jsonText));
ipcMain.handle('list-presets', () => readPresets());
ipcMain.handle('save-preset', (_event, name, jsonText) => savePreset(name, jsonText));
ipcMain.handle('delete-preset', (_event, name) => deletePreset(name));
