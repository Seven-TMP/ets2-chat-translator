const { app, BrowserWindow, dialog, ipcMain } = require('electron');
const fs = require('fs');
const path = require('path');
const childProcess = require('child_process');
const crypto = require('crypto');

const DLL_NAME = 'ets2_chat_translator.dll';
const PREVIEW_EXE_NAME = 'ets2_chat_translator_preview.exe';
const CONFIG_NAME = 'ets2_chat_translator_config.json';
const PRESETS_NAME = 'config_presets.json';
const UPDATE_SETTINGS_NAME = 'update_settings.json';
const UPDATE_REPO = 'Seven-TMP/ets2-chat-translator';
const VERSION_MANIFEST_URL = `https://raw.githubusercontent.com/${UPDATE_REPO}/main/version.json`;
const UPDATE_API_URL = `https://api.github.com/repos/${UPDATE_REPO}/releases/latest`;
const UPDATE_MIRROR_TEST_URL = `https://github.com/${UPDATE_REPO}/releases/latest`;
const DIRECT_PROXY_ID = 'direct';

const UPDATE_PROXIES = [
  'github.chenc.dev',
  'ghproxy.cfd',
  'github.tbedu.top',
  'ghproxy.cc',
  'gh.monlor.com',
  'cdn.akaere.online',
  'gh.idayer.com',
  'gh.llkk.cc',
  'ghpxy.hwinzniej.top',
  'github-proxy.memory-echoes.cn',
  'git.yylx.win',
  'gitproxy.mrhjx.cn',
  'gh.fhjhy.top',
  'gp.zkitefly.eu.org',
  'gh-proxy.com',
  'ghfile.geekertao.top',
  'j.1lin.dpdns.org',
  'ghproxy.imciel.com',
  'github-proxy.teach-english.tech',
  'gh.927223.xyz',
  'github.ednovas.xyz',
  'ghf.xn--eqrr82bzpe.top',
  'gh.dpik.top',
  'gh.jasonzeng.dev',
  'gh.xxooo.cf',
  'gh.bugdey.us.kg',
  'ghm.078465.xyz',
  'j.1win.ggff.net',
  'tvv.tw',
  'gitproxy.127731.xyz',
  'gh.inkchills.cn',
  'ghproxy.cxkpro.top',
  'gh.sixyin.com',
  'github.geekery.cn',
  'git.669966.xyz',
  'gh.5050net.cn',
  'gh.felicity.ac.cn',
  'github.dpik.top',
  'ghp.keleyaa.com',
  'gh.wsmdn.dpdns.org',
  'ghproxy.monkeyray.net',
  'fastgit.cc',
  'gh.catmak.name',
  'gh.noki.icu'
].map((host) => ({ id: host, label: host, baseUrl: `https://${host}` })).concat([
  { id: DIRECT_PROXY_ID, label: 'GitHub 直连', baseUrl: '' }
]);

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

function previewExePath() {
  const packaged = path.join(process.resourcesPath || '', PREVIEW_EXE_NAME);
  if (packaged && fs.existsSync(packaged)) return packaged;

  const besideExe = path.join(appRoot(), PREVIEW_EXE_NAME);
  if (fs.existsSync(besideExe)) return besideExe;

  return path.join(appRoot(), 'build', PREVIEW_EXE_NAME);
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

function updateSettingsPath() {
  return path.join(app.getPath('userData'), UPDATE_SETTINGS_NAME);
}

function defaultUpdateSettings() {
  return {
    proxyMode: 'auto',
    proxyId: UPDATE_PROXIES[0].id,
    customProxyUrl: '',
    lastFastProxyId: '',
    updatedAt: ''
  };
}

function readUpdateSettings() {
  const file = updateSettingsPath();
  if (!fs.existsSync(file)) return defaultUpdateSettings();
  try {
    return { ...defaultUpdateSettings(), ...JSON.parse(fs.readFileSync(file, 'utf8')) };
  } catch {
    return defaultUpdateSettings();
  }
}

function writeUpdateSettings(settings) {
  const next = {
    ...defaultUpdateSettings(),
    ...settings,
    updatedAt: new Date().toISOString()
  };
  fs.mkdirSync(path.dirname(updateSettingsPath()), { recursive: true });
  fs.writeFileSync(updateSettingsPath(), JSON.stringify(next, null, 2), 'utf8');
  return next;
}

function appVersion() {
  try {
    return app.getVersion();
  } catch {
    try {
      return require('../package.json').version || '0.0.0';
    } catch {
      return '0.0.0';
    }
  }
}

function parseVersion(value) {
  return String(value || '')
    .replace(/^v/i, '')
    .split(/[.+-]/)
    .slice(0, 3)
    .map((part) => Number.parseInt(part, 10) || 0);
}

function compareVersions(a, b) {
  const left = parseVersion(a);
  const right = parseVersion(b);
  for (let i = 0; i < 3; ++i) {
    if ((left[i] || 0) > (right[i] || 0)) return 1;
    if ((left[i] || 0) < (right[i] || 0)) return -1;
  }
  return 0;
}

function proxyById(id) {
  return UPDATE_PROXIES.find((item) => item.id === id) || UPDATE_PROXIES[0];
}

function normalizeProxyBaseUrl(value) {
  let text = String(value || '').trim();
  if (!text) return '';
  if (!/^https?:\/\//i.test(text)) text = `https://${text}`;
  const parsed = new URL(text);
  return `${parsed.protocol}//${parsed.host}${parsed.pathname.replace(/\/+$/, '')}`;
}

function customProxyFromUrl(value) {
  const baseUrl = normalizeProxyBaseUrl(value);
  if (!baseUrl) return null;
  const parsed = new URL(baseUrl);
  return {
    id: 'custom',
    label: parsed.host,
    baseUrl
  };
}

function proxyFromSettings(settings) {
  const current = { ...defaultUpdateSettings(), ...(settings || {}) };
  if (current.proxyMode === 'custom' && current.customProxyUrl) {
    try {
      const custom = customProxyFromUrl(current.customProxyUrl);
      if (custom) return custom;
    } catch (error) {
      throw new Error(`自定义镜像地址无效：${error.message}`);
    }
  }
  if (current.proxyMode === 'manual') return proxyById(current.proxyId);
  if (current.lastFastProxyId) return proxyById(current.lastFastProxyId);
  return proxyById(current.proxyId || DIRECT_PROXY_ID);
}

function proxiedUrl(originalUrl, proxy) {
  if (!proxy || proxy.id === DIRECT_PROXY_ID || !proxy.baseUrl) return originalUrl;
  return `${proxy.baseUrl.replace(/\/+$/, '')}/${originalUrl}`;
}

function requestBuffer(urlText, headers = {}, timeoutMs = 8000, redirects = 4) {
  return new Promise((resolve) => {
    const started = Date.now();
    let url;
    try {
      url = new URL(urlText);
    } catch (error) {
      resolve({ status: 0, body: Buffer.alloc(0), headers: {}, error: `bad url: ${error.message}`, elapsedMs: 0 });
      return;
    }

    const lib = url.protocol === 'http:' ? require('http') : require('https');
    const req = lib.request(url, {
      method: 'GET',
      headers,
      timeout: Math.max(1500, timeoutMs || 8000)
    }, (res) => {
      const status = res.statusCode || 0;
      const location = res.headers.location;
      if (status >= 300 && status < 400 && location && redirects > 0) {
        res.resume();
        const nextUrl = new URL(location, url).toString();
        requestBuffer(nextUrl, headers, timeoutMs, redirects - 1).then(resolve);
        return;
      }

      const chunks = [];
      let size = 0;
      res.on('data', (chunk) => {
        size += chunk.length;
        if (size <= 1024 * 1024) chunks.push(chunk);
      });
      res.on('end', () => {
        resolve({
          status,
          body: Buffer.concat(chunks),
          headers: res.headers,
          elapsedMs: Date.now() - started
        });
      });
    });
    req.on('timeout', () => req.destroy(new Error('timeout')));
    req.on('error', (error) => {
      resolve({ status: 0, body: Buffer.alloc(0), headers: {}, error: error.message, elapsedMs: Date.now() - started });
    });
    req.end();
  });
}

function requestJsonViaProxy(urlText, proxy, timeoutMs = 9000) {
  return requestBuffer(proxiedUrl(urlText, proxy), {
    Accept: 'application/vnd.github+json',
    'User-Agent': 'ETS2-Chat-Translator-Manager'
  }, timeoutMs).then((reply) => {
    if (reply.status < 200 || reply.status >= 300) {
      const body = reply.body.toString('utf8').slice(0, 500);
      throw new Error(reply.error || `HTTP ${reply.status}${body ? `: ${body}` : ''}`);
    }
    try {
      return {
        data: JSON.parse(reply.body.toString('utf8')),
        elapsedMs: reply.elapsedMs,
        proxy
      };
    } catch (error) {
      throw new Error(`JSON 解析失败：${error.message}`);
    }
  });
}

function requestHeadViaProxy(urlText, proxy, timeoutMs = 3500) {
  return new Promise((resolve) => {
    const started = Date.now();
    let url;
    try {
      url = new URL(proxiedUrl(urlText, proxy));
    } catch (error) {
      resolve({ status: 0, error: `bad url: ${error.message}`, elapsedMs: 0 });
      return;
    }

    const lib = url.protocol === 'http:' ? require('http') : require('https');
    const req = lib.request(url, {
      method: 'HEAD',
      headers: { 'User-Agent': 'ETS2-Chat-Translator-Manager' },
      timeout: Math.max(1200, timeoutMs || 3500)
    }, (res) => {
      res.resume();
      resolve({
        status: res.statusCode || 0,
        headers: res.headers,
        elapsedMs: Date.now() - started
      });
    });
    req.on('timeout', () => req.destroy(new Error('timeout')));
    req.on('error', (error) => {
      resolve({ status: 0, error: error.message, elapsedMs: Date.now() - started });
    });
    req.end();
  });
}

function downloadFileViaProxy(urlText, proxy, destination, onProgress, timeoutMs = 30000, redirects = 5) {
  return new Promise((resolve, reject) => {
    let url;
    try {
      url = new URL(proxiedUrl(urlText, proxy));
    } catch (error) {
      reject(new Error(`bad url: ${error.message}`));
      return;
    }

    fs.mkdirSync(path.dirname(destination), { recursive: true });
    const tempPath = `${destination}.download`;
    if (fs.existsSync(tempPath)) fs.unlinkSync(tempPath);

    const start = Date.now();
    const headers = {
      Accept: 'application/octet-stream',
      'User-Agent': 'ETS2-Chat-Translator-Manager'
    };

    const run = (targetUrl, redirectLeft) => {
      const target = new URL(targetUrl);
      const lib = target.protocol === 'http:' ? require('http') : require('https');
      const req = lib.request(target, { method: 'GET', headers, timeout: Math.max(5000, timeoutMs || 30000) }, (res) => {
        const status = res.statusCode || 0;
        const location = res.headers.location;
        if (status >= 300 && status < 400 && location && redirectLeft > 0) {
          res.resume();
          run(new URL(location, target).toString(), redirectLeft - 1);
          return;
        }
        if (status < 200 || status >= 300) {
          res.resume();
          reject(new Error(`HTTP ${status}`));
          return;
        }

        const total = Number.parseInt(res.headers['content-length'] || '0', 10) || 0;
        let downloaded = 0;
        const file = fs.createWriteStream(tempPath);
        res.on('data', (chunk) => {
          downloaded += chunk.length;
          if (onProgress) onProgress({ downloaded, total, percent: total ? Math.round(downloaded * 100 / total) : 0 });
        });
        res.pipe(file);
        file.on('finish', () => {
          file.close(() => {
            if (fs.existsSync(destination)) fs.unlinkSync(destination);
            fs.renameSync(tempPath, destination);
            resolve({ path: destination, bytes: downloaded, elapsedMs: Date.now() - start, proxy });
          });
        });
        file.on('error', reject);
      });
      req.on('timeout', () => req.destroy(new Error('timeout')));
      req.on('error', reject);
      req.end();
    };

    run(url.toString(), redirects);
  }).catch((error) => {
    const tempPath = `${destination}.download`;
    if (fs.existsSync(tempPath)) {
      try { fs.unlinkSync(tempPath); } catch {}
    }
    throw error;
  });
}

function readPresets() {
  const file = presetsPath();
  if (!fs.existsSync(file)) return [];
  try {
    const parsed = JSON.parse(fs.readFileSync(file, 'utf8'));
    return Array.isArray(parsed.presets)
      ? parsed.presets
        .filter((item) => item && item.name && item.config)
        .map((item) => ({ ...item, config: transformProviderSecrets(item.config, 'decrypt') }))
      : [];
  } catch {
    return [];
  }
}

function writePresets(presets) {
  const file = presetsPath();
  fs.mkdirSync(path.dirname(file), { recursive: true });
  const stored = presets.map((item) => ({ ...item, config: transformProviderSecrets(item.config, 'encrypt') }));
  fs.writeFileSync(file, JSON.stringify({ presets: stored }, null, 2), 'utf8');
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

const CONFIG_SECRET_PREFIX = 'enc:dpapi:';
const CONFIG_SECRET_FIELDS = ['api_key', 'api_secret', 'secret_key'];

function runProtectedDataBatch(action, values) {
  const prelude = `
Add-Type -AssemblyName System.Security
[Console]::InputEncoding = [Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$items = [Console]::In.ReadToEnd() | ConvertFrom-Json
$out = @()
`;
  const script = action === 'protect'
    ? `${prelude}
foreach ($plain in $items) {
  $bytes = [Text.Encoding]::UTF8.GetBytes([string]$plain)
  $protected = [Security.Cryptography.ProtectedData]::Protect($bytes, $null, [Security.Cryptography.DataProtectionScope]::CurrentUser)
  $out += [Convert]::ToBase64String($protected)
}
[Console]::Out.Write(($out | ConvertTo-Json -Compress))
`
    : `${prelude}
foreach ($cipher in $items) {
  $bytes = [Convert]::FromBase64String(([string]$cipher).Trim())
  $plain = [Security.Cryptography.ProtectedData]::Unprotect($bytes, $null, [Security.Cryptography.DataProtectionScope]::CurrentUser)
  $out += [Text.Encoding]::UTF8.GetString($plain)
}
[Console]::Out.Write(($out | ConvertTo-Json -Compress))
`;
  try {
    const output = childProcess.execFileSync('powershell.exe', [
      '-NoProfile',
      '-NonInteractive',
      '-ExecutionPolicy',
      'Bypass',
      '-Command',
      script
    ], {
      input: JSON.stringify(values || []),
      encoding: 'utf8',
      windowsHide: true,
      maxBuffer: 1024 * 1024
    });
    const parsed = JSON.parse(output || '[]');
    return Array.isArray(parsed) ? parsed : [parsed];
  } catch (error) {
    const detail = error.stderr ? String(error.stderr).trim() : error.message;
    throw new Error(`密钥${action === 'protect' ? '加密' : '解密'}失败：${detail}`);
  }
}

function runProtectedData(action, value) {
  return runProtectedDataBatch(action, [String(value || '')])[0] || '';
}

function isEncryptedSecret(value) {
  return typeof value === 'string' && value.startsWith(CONFIG_SECRET_PREFIX);
}

function encryptConfigSecret(value) {
  if (typeof value !== 'string' || !value || isEncryptedSecret(value)) return value || '';
  return `${CONFIG_SECRET_PREFIX}${runProtectedData('protect', value)}`;
}

function decryptConfigSecret(value) {
  if (!isEncryptedSecret(value)) return value || '';
  return runProtectedData('unprotect', value.slice(CONFIG_SECRET_PREFIX.length));
}

function transformProviderSecrets(config, mode) {
  const next = JSON.parse(JSON.stringify(config || {}));
  const targets = [];
  for (const provider of next.providers || []) {
    for (const field of CONFIG_SECRET_FIELDS) {
      if (typeof provider[field] !== 'string' || !provider[field]) continue;
      const value = provider[field];
      if (mode === 'encrypt' && !isEncryptedSecret(value)) {
        targets.push({ provider, field, value });
      } else if (mode === 'decrypt' && isEncryptedSecret(value)) {
        targets.push({ provider, field, value: value.slice(CONFIG_SECRET_PREFIX.length) });
      }
    }
  }
  if (!targets.length) return next;

  const transformed = runProtectedDataBatch(mode === 'encrypt' ? 'protect' : 'unprotect', targets.map((item) => item.value));
  targets.forEach((item, index) => {
    item.provider[item.field] = mode === 'encrypt'
      ? `${CONFIG_SECRET_PREFIX}${transformed[index] || ''}`
      : (transformed[index] || '');
  });
  return next;
}

function decryptConfigText(jsonText) {
  if (!jsonText) return jsonText;
  return JSON.stringify(transformProviderSecrets(JSON.parse(jsonText), 'decrypt'), null, 2);
}

function encryptConfigText(jsonText) {
  return JSON.stringify(transformProviderSecrets(JSON.parse(jsonText), 'encrypt'), null, 2);
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
  if (isLikelyOllamaUrl(u)) normalizeOllamaOpenAIBaseUrl(u);
  const prefix = u.pathname.replace(/\/+$/, '');
  return `${u.protocol}//${u.host}${prefix}${pathWithQuery}`;
}

function isLikelyOllamaUrl(url) {
  return url.port === '11434' || url.hostname === 'ollama' || url.hostname.includes('ollama');
}

function normalizeOllamaOpenAIBaseUrl(url) {
  const pathName = url.pathname.replace(/\/+$/, '');
  if (!pathName || pathName === '/' || pathName === '/api' || pathName.startsWith('/api/')) {
    url.pathname = '/v1';
  }
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
      timeout: Math.max(1500, Math.min(30000, timeoutMs || 5000))
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
  const value = String(target || 'zh-CN').trim().toLowerCase().replace(/_/g, '-');
  if (['zh', 'zh-cn', 'zh-hans', 'zh-chs'].includes(value)) return 'zh';
  if (['zh-tw', 'zh-hant', 'cht'].includes(value)) return 'zh-TW';
  return value;
}

function targetMicrosoft(target) {
  const value = String(target || 'zh-CN').trim().toLowerCase().replace(/_/g, '-');
  if (['zh', 'zh-cn', 'zh-hans', 'zh-chs'].includes(value)) return 'zh-Hans';
  if (['zh-tw', 'zh-hant', 'cht'].includes(value)) return 'zh-Hant';
  return value;
}

function targetBaidu(target) {
  const value = String(target || 'zh-CN').trim().toLowerCase().replace(/_/g, '-');
  if (['zh', 'zh-cn', 'zh-hans', 'zh-chs'].includes(value)) return 'zh';
  if (['zh-tw', 'zh-hant', 'cht'].includes(value)) return 'cht';
  if (value === 'ja') return 'jp';
  return value;
}

function sourceBaidu(source) {
  const value = String(source || 'auto').trim().toLowerCase().replace(/_/g, '-');
  if (['zh-cn', 'zh-hans', 'zh-chs'].includes(value)) return 'zh';
  if (['zh-tw', 'zh-hant', 'cht'].includes(value)) return 'cht';
  if (value === 'ja') return 'jp';
  return value || 'auto';
}

function targetYoudao(target) {
  const value = String(target || 'zh-CN').trim().toLowerCase().replace(/_/g, '-');
  if (['zh', 'zh-cn', 'zh-hans', 'zh-chs'].includes(value)) return 'zh-CHS';
  if (['zh-tw', 'zh-hant', 'cht'].includes(value)) return 'zh-CHT';
  return value;
}

function targetDeepL(target) {
  const value = String(target || 'zh-CN').trim().toLowerCase().replace(/_/g, '-');
  if (['zh', 'zh-cn', 'zh-hans', 'zh-chs'].includes(value)) return 'ZH';
  if (['zh-tw', 'zh-hant', 'cht'].includes(value)) return 'ZH-HANT';
  return value.toUpperCase();
}

function sourceOrAuto(provider) {
  return provider.source || provider.source_lang || 'auto';
}

function providerName(provider) {
  return provider.label || provider.kind || 'Provider';
}

function providerProbe(provider) {
  return [
    provider.kind,
    provider.label,
    provider.base_url,
    provider.baseUrl,
    provider.model
  ].filter(Boolean).join(' ').toLowerCase();
}

function isDeepSeekProvider(provider) {
  const kind = String(provider.kind || '').toLowerCase();
  return ['deepseek', 'deepseek_chat', 'deepseek_compatible'].includes(kind);
}

function isMiMoProvider(provider) {
  const probe = providerProbe(provider);
  return probe.includes('mimo') || probe.includes('xiaomi') || probe.includes('xiaomimimo');
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
  const bodyObject = {
    model: provider.model,
    temperature: 0,
    messages: [
      { role: 'system', content: `Translate chat into ${runtime.target}. Output only the translation.` },
      { role: 'user', content: runtime.sampleText }
    ]
  };
  if (isMiMoProvider(provider)) {
    bodyObject.max_completion_tokens = 96;
  } else {
    bodyObject.max_tokens = 96;
  }
  if (isDeepSeekProvider(provider) || isMiMoProvider(provider)) {
    bodyObject.thinking = { type: 'disabled' };
  }
  const body = JSON.stringify(bodyObject);
  const headers = {
    'Content-Type': 'application/json',
    Accept: 'application/json'
  };
  if (provider.api_key) headers.Authorization = `Bearer ${provider.api_key}`;
  const fallbackBaseUrl = isDeepSeekProvider(provider)
    ? 'https://api.deepseek.com'
    : isMiMoProvider(provider)
      ? 'https://api.xiaomimimo.com/v1'
    : 'https://api.openai.com/v1';
  return finishProviderTest(provider, runtime, () => requestText('POST',
    joinProviderUrl(provider.base_url, fallbackBaseUrl, '/chat/completions'),
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
    from: sourceBaidu(sourceOrAuto(provider)),
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
    if (kind === 'deepseek' || kind === 'deepseek_chat' || kind === 'deepseek_compatible' ||
        kind === 'openai_compatible' || kind === 'openai' || kind === 'chat_completions') return await testOpenAI(provider, runtime);
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
  const config = transformProviderSecrets(JSON.parse(jsonText), 'decrypt');
  const providers = (config.providers || []).filter((provider) => provider && provider.enabled && provider.kind);
  const runtime = {
    target: config.target_lang || 'zh-CN',
    timeoutMs: Math.max(1500, Math.min(30000, config.timeout_ms || 5000)),
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

function formatBytes(value) {
  const size = Number(value) || 0;
  if (size >= 1024 * 1024) return `${(size / 1024 / 1024).toFixed(1)} MB`;
  if (size >= 1024) return `${(size / 1024).toFixed(1)} KB`;
  return `${size} B`;
}

function releaseAsset(release) {
  const assets = Array.isArray(release?.assets) ? release.assets : [];
  return assets.find((asset) => /\.exe$/i.test(asset.name || '') && /setup|manager/i.test(asset.name || '')) ||
    assets.find((asset) => /\.exe$/i.test(asset.name || '')) ||
    null;
}

function manifestToRelease(manifest) {
  const version = manifest.version || manifest.latestVersion || manifest.tag_name || manifest.tagName || '';
  const tag = String(manifest.tag_name || manifest.tagName || version || '').replace(/^([^v])/i, 'v$1');
  const downloadUrl = manifest.download_url || manifest.downloadUrl || manifest.url || manifest.installer || '';
  const assetName = manifest.asset_name || manifest.assetName ||
    (downloadUrl ? path.basename(new URL(downloadUrl).pathname) : `ETS2-Chat-Translator-Manager-Setup-${String(version).replace(/^v/i, '')}.exe`);
  return {
    tag_name: tag,
    name: manifest.name || tag,
    body: manifest.body || manifest.notes || manifest.changelog || '',
    html_url: manifest.html_url || manifest.htmlUrl || `https://github.com/${UPDATE_REPO}/releases/tag/${tag}`,
    published_at: manifest.published_at || manifest.publishedAt || '',
    assets: downloadUrl ? [{
      name: assetName,
      size: manifest.size || 0,
      browser_download_url: downloadUrl
    }] : []
  };
}

function releaseSummary(release, proxy, elapsedMs) {
  const tag = release.tag_name || release.name || '';
  const asset = releaseAsset(release);
  return {
    currentVersion: appVersion(),
    latestVersion: tag.replace(/^v/i, ''),
    tagName: tag,
    hasUpdate: compareVersions(tag, appVersion()) > 0,
    name: release.name || tag,
    body: release.body || '',
    htmlUrl: release.html_url || `https://github.com/${UPDATE_REPO}/releases/latest`,
    publishedAt: release.published_at || release.created_at || '',
    proxyId: proxy?.id || DIRECT_PROXY_ID,
    proxyLabel: proxy?.label || 'GitHub 直连',
    elapsedMs,
    asset: asset ? {
      name: asset.name,
      size: asset.size || 0,
      sizeText: formatBytes(asset.size || 0),
      downloadUrl: asset.browser_download_url || asset.url || ''
    } : null
  };
}

async function requestReleaseViaProxy(proxy, timeoutMs = 9000) {
  try {
    const manifestResult = await requestJsonViaProxy(VERSION_MANIFEST_URL, proxy, timeoutMs);
    return {
      release: manifestToRelease(manifestResult.data),
      elapsedMs: manifestResult.elapsedMs,
      proxy,
      source: 'version.json'
    };
  } catch (manifestError) {
    const apiResult = await requestJsonViaProxy(UPDATE_API_URL, proxy, timeoutMs);
    return {
      release: apiResult.data,
      elapsedMs: apiResult.elapsedMs,
      proxy,
      source: 'github-api',
      manifestError: manifestError.message
    };
  }
}

async function testUpdateProxy(proxy, timeoutMs = 7000) {
  const started = Date.now();
  const result = await requestHeadViaProxy(UPDATE_MIRROR_TEST_URL, proxy, timeoutMs);
  const status = result.status || 0;
  const ok = (status >= 200 && status < 400) || status === 405;
  const rawError = result.error || (status ? `HTTP ${status}` : '未返回状态');
  return {
    id: proxy.id,
    label: proxy.label,
    ok,
    elapsedMs: result.elapsedMs || (Date.now() - started),
    status,
    tagName: '',
    source: 'head',
    error: ok ? '' : String(/timeout/i.test(rawError) ? '超时' : rawError).slice(0, 180)
  };
}

async function speedTestUpdateProxies(saveFast = true) {
  const proxies = [...UPDATE_PROXIES];
  const concurrency = 10;
  const results = new Array(proxies.length);
  let cursor = 0;
  const workerCount = Math.min(concurrency, proxies.length);
  const workers = Array.from({ length: workerCount }, async () => {
    while (cursor < proxies.length) {
      const index = cursor++;
      const proxy = proxies[index];
      results[index] = await testUpdateProxy(proxy, proxy.id === DIRECT_PROXY_ID ? 5000 : 3500);
    }
  });
  await Promise.all(workers);

  results.sort((a, b) => {
    if (a.ok !== b.ok) return a.ok ? -1 : 1;
    if (a.ok && b.ok) return a.elapsedMs - b.elapsedMs;
    const leftTimedOut = /timeout|超时/i.test(a.error || '');
    const rightTimedOut = /timeout|超时/i.test(b.error || '');
    if (leftTimedOut !== rightTimedOut) return leftTimedOut ? 1 : -1;
    return proxies.findIndex((proxy) => proxy.id === a.id) - proxies.findIndex((proxy) => proxy.id === b.id);
  });

  const fastest = results.find((item) => item.ok) || null;
  if (saveFast && fastest) {
    const settings = readUpdateSettings();
    writeUpdateSettings({ ...settings, lastFastProxyId: fastest.id });
  }
  return { fastest, results };
}

async function chooseUpdateProxy() {
  const settings = readUpdateSettings();
  if (settings.proxyMode === 'manual' || settings.proxyMode === 'custom' || settings.lastFastProxyId) {
    return proxyFromSettings(settings);
  }
  const tested = await speedTestUpdateProxies(true);
  return proxyById(tested.fastest?.id || DIRECT_PROXY_ID);
}

async function checkForUpdates(forceSpeedTest = false) {
  let proxy = await chooseUpdateProxy();
  let speedTest = null;
  if (forceSpeedTest) {
    speedTest = await speedTestUpdateProxies(true);
    if (speedTest.fastest) proxy = proxyById(speedTest.fastest.id);
  }

  const result = await requestReleaseViaProxy(proxy, proxy.id === DIRECT_PROXY_ID ? 10000 : 8000);
  return {
    ...releaseSummary(result.release, proxy, result.elapsedMs),
    source: result.source,
    manifestError: result.manifestError || '',
    settings: readUpdateSettings(),
    proxies: UPDATE_PROXIES,
    speedTest
  };
}

async function downloadAndInstallUpdate(downloadUrl, fileName, proxyId) {
  if (!downloadUrl || !/^https?:\/\//i.test(downloadUrl)) throw new Error('缺少有效下载地址');
  const settings = readUpdateSettings();
  const proxy = proxyId === 'custom'
    ? proxyFromSettings({ ...settings, proxyMode: 'custom' })
    : proxyId ? proxyById(proxyId) : await chooseUpdateProxy();
  const cleanName = String(fileName || path.basename(new URL(downloadUrl).pathname) || 'ETS2-Chat-Translator-Manager-Setup.exe')
    .replace(/[<>:"/\\|?*]/g, '_');
  const destination = path.join(app.getPath('downloads'), cleanName);
  const result = await downloadFileViaProxy(downloadUrl, proxy, destination, (progress) => {
    if (!mainWindow || mainWindow.isDestroyed()) return;
    mainWindow.webContents.send('update-download-progress', {
      ...progress,
      downloadedText: formatBytes(progress.downloaded),
      totalText: progress.total ? formatBytes(progress.total) : '',
      fileName: cleanName,
      proxyId: proxy.id,
      proxyLabel: proxy.label
    });
  });

  childProcess.spawn(result.path, [], {
    detached: true,
    stdio: 'ignore',
    windowsHide: false
  }).unref();

  return {
    path: result.path,
    bytes: result.bytes,
    bytesText: formatBytes(result.bytes),
    elapsedMs: result.elapsedMs,
    proxyId: proxy.id,
    proxyLabel: proxy.label
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
  return decryptConfigText(fs.readFileSync(file, 'utf8'));
});

ipcMain.handle('write-config', (_event, game, ets2Path, jsonText) => {
  const def = gameDef(game);
  if (!looksLikeGame(ets2Path, game)) throw new Error(`请先选择有效的 ${def.shortName} 安装目录`);
  const file = configPath(ets2Path);
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, encryptConfigText(jsonText), 'utf8');
  return installState(game, ets2Path);
});

ipcMain.handle('test-config', (_event, jsonText) => testConfigText(jsonText));
ipcMain.handle('preview-overlay', () => {
  const exe = previewExePath();
  if (!fs.existsSync(exe)) {
    throw new Error(`预览程序不存在：${exe}`);
  }
  childProcess.spawn(exe, [], {
    detached: true,
    stdio: 'ignore',
    windowsHide: false
  }).unref();
  return { ok: true, path: exe };
});
ipcMain.handle('list-presets', () => readPresets());
ipcMain.handle('save-preset', (_event, name, jsonText) => savePreset(name, jsonText));
ipcMain.handle('delete-preset', (_event, name) => deletePreset(name));
ipcMain.handle('get-update-options', () => ({
  version: appVersion(),
  settings: readUpdateSettings(),
  proxies: UPDATE_PROXIES
}));
ipcMain.handle('save-update-settings', (_event, settings) => writeUpdateSettings(settings || {}));
ipcMain.handle('speed-test-update-proxies', () => speedTestUpdateProxies(true));
ipcMain.handle('check-update', (_event, forceSpeedTest) => checkForUpdates(!!forceSpeedTest));
ipcMain.handle('download-update', (_event, downloadUrl, fileName, proxyId) => downloadAndInstallUpdate(downloadUrl, fileName, proxyId));
