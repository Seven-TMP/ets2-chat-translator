const els = {
  statusPill: document.querySelector('#statusPill'),
  installState: document.querySelector('#installState'),
  pluginDir: document.querySelector('#pluginDir'),
  ets2Path: document.querySelector('#ets2Path'),
  detectBtn: document.querySelector('#detectBtn'),
  browseBtn: document.querySelector('#browseBtn'),
  installBtn: document.querySelector('#installBtn'),
  uninstallBtn: document.querySelector('#uninstallBtn'),
  gamePathLabel: document.querySelector('#gamePathLabel'),
  configPreset: document.querySelector('#configPreset'),
  presetName: document.querySelector('#presetName'),
  applyPresetBtn: document.querySelector('#applyPresetBtn'),
  savePresetBtn: document.querySelector('#savePresetBtn'),
  deletePresetBtn: document.querySelector('#deletePresetBtn'),
  preset: document.querySelector('#preset'),
  targetLang: document.querySelector('#targetLang'),
  overlayHotkey: document.querySelector('#overlayHotkey'),
  overlayOpacity: document.querySelector('#overlayOpacity'),
  overlayOpacityValue: document.querySelector('#overlayOpacityValue'),
  workers: document.querySelector('#workers'),
  queueLimit: document.querySelector('#queueLimit'),
  cacheLimit: document.querySelector('#cacheLimit'),
  timeoutMs: document.querySelector('#timeoutMs'),
  fontSize: document.querySelector('#fontSize'),
  fontSizeValue: document.querySelector('#fontSizeValue'),
  fontSizeMinus: document.querySelector('#fontSizeMinus'),
  fontSizePlus: document.querySelector('#fontSizePlus'),
  fontPreview: document.querySelector('#fontPreview'),
  llmFields: document.querySelector('#llmFields'),
  providerLabel: document.querySelector('#providerLabel'),
  baseUrl: document.querySelector('#baseUrl'),
  apiKey: document.querySelector('#apiKey'),
  apiSecret: document.querySelector('#apiSecret'),
  model: document.querySelector('#model'),
  sourceLang: document.querySelector('#sourceLang'),
  enableFallbacks: document.querySelector('#enableFallbacks'),
  loadConfigBtn: document.querySelector('#loadConfigBtn'),
  overlayPreviewBtn: document.querySelector('#overlayPreviewBtn'),
  previewBtn: document.querySelector('#previewBtn'),
  testConfigBtn: document.querySelector('#testConfigBtn'),
  saveConfigBtn: document.querySelector('#saveConfigBtn'),
  testResult: document.querySelector('#testResult'),
  preview: document.querySelector('#preview'),
  updateState: document.querySelector('#updateState'),
  mirrorUrl: document.querySelector('#mirrorUrl'),
  saveMirrorBtn: document.querySelector('#saveMirrorBtn'),
  clearMirrorBtn: document.querySelector('#clearMirrorBtn'),
  mirrorList: document.querySelector('#mirrorList'),
  speedTestBtn: document.querySelector('#speedTestBtn'),
  checkUpdateBtn: document.querySelector('#checkUpdateBtn'),
  useDirectBtn: document.querySelector('#useDirectBtn'),
  useSelectedMirrorBtn: document.querySelector('#useSelectedMirrorBtn'),
  downloadUpdateBtn: document.querySelector('#downloadUpdateBtn'),
  updateSummary: document.querySelector('#updateSummary'),
  updateProgress: document.querySelector('#updateProgress'),
  updateProgressBar: document.querySelector('#updateProgressBar'),
  updateProgressText: document.querySelector('#updateProgressText'),
  releaseNotes: document.querySelector('#releaseNotes'),
  mirrorSelectArea: document.querySelector('#mirrorSelectArea'),
  updateResult: document.querySelector('#updateResult'),
  updatePlaceholder: document.querySelector('#updatePlaceholder')
};

let currentGame = 'ets2';
let configPresets = [];
let updateInfo = null;
let updateOptions = null;
let selectedMirrorId = null; // mirror user selected from the list
const GAME_LABELS = {
  ets2: 'ETS2',
  ats: 'ATS'
};

function numberValue(input, fallback) {
  const value = Number.parseInt(input.value, 10);
  return Number.isFinite(value) ? value : fallback;
}

const PROVIDER_PRESETS = {
  'anthropic': {
    kind: 'anthropic',
    label: 'Anthropic',
    baseUrl: 'https://api.anthropic.com/v1',
    model: 'claude-3-5-haiku-latest',
    needsModel: true,
    needsApiKey: true
  },
  'openai': {
    kind: 'openai_compatible',
    label: 'OpenAI',
    baseUrl: 'https://api.openai.com/v1',
    model: 'gpt-4o-mini',
    needsModel: true,
    needsApiKey: true
  },
  'ollama': {
    kind: 'openai_compatible',
    label: 'Ollama',
    baseUrl: 'http://localhost:11434/v1',
    model: 'translategemma:27b',
    needsModel: true,
    needsApiKey: false,
    defaultWorkers: 1,
    defaultTimeoutMs: 30000
  },
  'deepseek': {
    kind: 'deepseek',
    label: 'DeepSeek',
    baseUrl: 'https://api.deepseek.com',
    model: 'deepseek-v4-flash',
    needsModel: true,
    needsApiKey: true
  },
  'deepl': {
    kind: 'deepl',
    label: 'DeepL',
    baseUrl: 'https://api-free.deepl.com',
    model: '',
    needsModel: false,
    needsApiKey: true
  },
  'google-cloud': {
    kind: 'google_cloud',
    label: 'Google Cloud Translate',
    baseUrl: 'https://translation.googleapis.com',
    model: '',
    needsModel: false,
    needsApiKey: true
  },
  'microsoft': {
    kind: 'microsoft',
    label: 'Microsoft Translator',
    baseUrl: 'https://api.cognitive.microsofttranslator.com',
    model: '',
    needsModel: false,
    needsApiKey: true,
    needsSecret: true,
    secretLabel: 'Region'
  },
  'baidu': {
    kind: 'baidu',
    label: '百度翻译',
    baseUrl: 'https://fanyi-api.baidu.com',
    model: '',
    needsModel: false,
    needsApiKey: true,
    needsSecret: true
  },
  'youdao': {
    kind: 'youdao',
    label: '有道智云',
    baseUrl: 'https://openapi.youdao.com',
    model: '',
    needsModel: false,
    needsApiKey: true,
    needsSecret: true
  },
  'tencent': {
    kind: 'tencent',
    label: '腾讯云机器翻译',
    baseUrl: 'https://tmt.tencentcloudapi.com',
    model: 'ap-guangzhou',
    needsModel: true,
    modelLabel: 'Region',
    needsApiKey: true,
    needsSecret: true
  },
  'aliyun': {
    kind: 'aliyun',
    label: '阿里云机器翻译',
    baseUrl: 'https://mt.cn-hangzhou.aliyuncs.com',
    model: 'general',
    needsModel: true,
    modelLabel: 'Scene',
    needsApiKey: true,
    needsSecret: true,
    secretLabel: 'AccessKey Secret'
  },
  'volcengine': {
    kind: 'volcengine',
    label: '火山翻译',
    baseUrl: 'https://translate.volcengineapi.com',
    model: 'cn-north-1',
    needsModel: true,
    modelLabel: 'Region',
    needsApiKey: true,
    needsSecret: true,
    secretLabel: 'Secret Access Key'
  },
  'libretranslate': {
    kind: 'libretranslate',
    label: 'LibreTranslate',
    baseUrl: 'https://libretranslate.com',
    model: '',
    needsModel: false,
    needsApiKey: false
  },
  'free': {
    kind: '',
    label: '',
    baseUrl: '',
    model: '',
    needsModel: false,
    needsApiKey: false
  }
};

function fallbackProviders(target) {
  return [
    { kind: 'mymemory', label: 'MyMemory', enabled: true, source: 'auto', target },
    { kind: 'andeer', label: 'Andeer', enabled: false, source: 'auto', target }
  ];
}

function baseConfig() {
  const target = els.targetLang.value.trim() || 'zh-CN';
  return {
    target_lang: target,
    overlay_hotkey: els.overlayHotkey.value.trim() || 'Ctrl+Shift+T',
    workers: Math.max(1, Math.min(32, numberValue(els.workers, 8))),
    queue_limit: Math.max(50, numberValue(els.queueLimit, 1000)),
    cache_limit: Math.max(100, numberValue(els.cacheLimit, 1500)),
    timeout_ms: Math.max(1500, Math.min(30000, numberValue(els.timeoutMs, 5000))),
    font_size: Math.max(12, Math.min(28, numberValue(els.fontSize, 18))),
    overlay_opacity: Math.max(0, Math.min(100, numberValue(els.overlayOpacity, 98))),
    providers: []
  };
}

function buildConfig() {
  const config = baseConfig();
  const target = config.target_lang;
  const selected = PROVIDER_PRESETS[els.preset.value] || PROVIDER_PRESETS.free;
  if (selected.kind) {
    config.providers.unshift({
      kind: selected.kind,
      label: els.providerLabel.value.trim() || selected.label,
      enabled: true,
      base_url: normalizeOpenAIBaseUrl(els.baseUrl.value.trim(), selected),
      api_key: els.apiKey.value,
      api_secret: els.apiSecret.value,
      model: selected.needsModel ? (els.model.value.trim() || selected.model) : '',
      source: els.sourceLang.value.trim() || 'auto',
      target
    });
  }
  if (els.enableFallbacks.checked || !selected.kind) {
    config.providers.push(...fallbackProviders(target));
  }
  return config;
}

function setStatus(text) {
  els.statusPill.textContent = text;
}

function escapeHtml(value) {
  return String(value ?? '')
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

function normalizeOpenAIBaseUrl(value, selected = {}) {
  const raw = String(value || '').trim();
  if (!raw) return raw;
  const probe = `${selected.label || ''} ${selected.kind || ''} ${selected.model || ''} ${raw}`.toLowerCase();
  const likelyOllama = probe.includes('ollama') || probe.includes('translategemma') || /(^|[/:])11434(\/|$)/.test(raw);
  if (!likelyOllama) return raw;

  try {
    const url = new URL(raw);
    const path = url.pathname.replace(/\/+$/, '');
    if (!path || path === '/') url.pathname = '/v1';
    else if (path === '/api' || path.startsWith('/api/')) url.pathname = '/v1';
    return url.toString().replace(/\/$/, '');
  } catch {
    return raw;
  }
}

function renderTestResult(result) {
  const rows = (result.results || []).map((item) => {
    const state = item.ok ? 'ok' : 'fail';
    const status = item.status ? `HTTP ${item.status}` : '未请求';
    const elapsed = item.elapsed_ms ? `${item.elapsed_ms}ms` : '-';
    const error = item.error ? `<div class="test-error">${escapeHtml(item.error)}</div>` : '';
    return `
      <div class="test-row ${state}">
        <div class="test-main">
          <span class="test-dot"></span>
          <strong>${escapeHtml(item.label)}</strong>
          <span>${escapeHtml(item.kind)}</span>
        </div>
        <div class="test-meta">${status} · ${elapsed}</div>
        ${error}
      </div>
    `;
  }).join('');

  els.testResult.innerHTML = `
    <div class="test-summary ${result.ok ? 'ok' : 'fail'}">${escapeHtml(result.summary || '测试完成')}</div>
    ${rows}
  `;
}

function renderMarkdownLite(value) {
  const text = String(value || '').trim();
  if (!text) return '<p>这个 Release 暂时没有填写更新日志。</p>';
  return text
    .split(/\r?\n/)
    .map((line) => {
      const clean = escapeHtml(line.trim());
      if (!clean) return '';
      if (/^#{1,6}\s+/.test(line)) return `<strong>${clean.replace(/^#+\s*/, '')}</strong>`;
      if (/^[-*]\s+/.test(line)) return `<p>• ${clean.replace(/^[-*]\s*/, '')}</p>`;
      if (/^\d+\.\s+/.test(line)) return `<p>${clean}</p>`;
      return `<p>${clean}</p>`;
    })
    .join('');
}

function mirrorLabel(id, customUrl = '') {
  if (id === 'custom') {
    try {
      return new URL(customUrl).host;
    } catch {
      return customUrl || '自定义镜像';
    }
  }
  const proxy = (updateOptions?.proxies || []).find((item) => item.id === id);
  return proxy?.label || '自动测速选择';
}

function formatMirrorTime(item) {
  if (item.ok) return `${(item.elapsedMs / 1000).toFixed(item.elapsedMs >= 1000 ? 3 : 2).replace(/0+$/, '').replace(/\.$/, '')}s`;
  if (item.skipped) return '待测';
  if (/timeout|超时/i.test(item.error || '')) return '超时';
  return '失败';
}

function renderMirrorList(results = []) {
  const settings = updateOptions?.settings || {};
  const measured = new Map(results.map((item) => [item.id, item]));
  const sourceProxies = updateOptions?.proxies || [];
  const proxyOrder = results.length
    ? [
        ...results
          .map((item) => sourceProxies.find((proxy) => proxy.id === item.id))
          .filter(Boolean),
        ...sourceProxies.filter((proxy) => !measured.has(proxy.id))
      ]
    : sourceProxies;

  // Build rows with speed results (if any) and mirror use buttons
  const rows = proxyOrder.map((proxy, index) => {
    const item = measured.get(proxy.id) || {
      id: proxy.id,
      label: proxy.label,
      ok: false,
      skipped: true,
      elapsedMs: 0,
      error: '待测'
    };
    const selClass = selectedMirrorId === proxy.id ? ' selected' : '';
    const state = item.ok ? 'ok' : item.skipped ? 'idle' : 'fail';
    return `
      <div class="mirror-row${selClass}" data-proxy-id="${escapeHtml(proxy.id)}">
        <span class="mirror-rank">${index + 1}</span>
        <span class="mirror-name">${escapeHtml(proxy.label)}</span>
        <span class="mirror-time ${state}">${escapeHtml(formatMirrorTime(item))}</span>
        <button class="mirror-use-btn secondary-btn" type="button" data-proxy-id="${escapeHtml(proxy.id)}">选择</button>
      </div>
    `;
  }).join('');

  // Custom proxy row
  const customUrl = settings.customProxyUrl;
  const customRow = customUrl ? `
    <div class="mirror-row${selectedMirrorId === 'custom' ? ' selected' : ''}" data-proxy-id="custom">
      <span class="mirror-rank">*</span>
      <span class="mirror-name">${escapeHtml(mirrorLabel('custom', customUrl))}</span>
      <span class="mirror-time idle">自定义</span>
      <button class="mirror-use-btn secondary-btn" type="button" data-proxy-id="custom">选择</button>
    </div>
  ` : '';

  els.mirrorList.innerHTML = customRow + rows;

  // Click on row selects it
  els.mirrorList.querySelectorAll('.mirror-row').forEach((row) => {
    row.addEventListener('click', () => {
      selectMirror(row.dataset.proxyId);
    });
  });

  // Click on "选择" button also selects
  els.mirrorList.querySelectorAll('.mirror-use-btn').forEach((btn) => {
    btn.addEventListener('click', (e) => {
      e.stopPropagation();
      selectMirror(btn.dataset.proxyId);
    });
  });
}

function selectMirror(proxyId) {
  selectedMirrorId = proxyId;
  // Update visual
  els.mirrorList.querySelectorAll('.mirror-row').forEach((row) => {
    row.classList.toggle('selected', row.dataset.proxyId === proxyId);
  });
  setStatus(`已选择镜像：${mirrorLabel(proxyId, updateOptions?.settings?.customProxyUrl)}`);
}

// Show mirror selection area, hide placeholder and previous results
function showMirrorSelection() {
  els.mirrorSelectArea.style.display = '';
  els.updateResult.style.display = 'none';
  els.updatePlaceholder.style.display = 'none';
  els.updateProgress.hidden = true;
  els.downloadUpdateBtn.disabled = true;
  selectedMirrorId = null;
  renderMirrorList(updateOptions?.lastSpeedResults || []);
}

// Show update check result
function showUpdateResult() {
  els.mirrorSelectArea.style.display = 'none';
  els.updateResult.style.display = '';
  els.updatePlaceholder.style.display = 'none';
}

function renderUpdateInfo(info) {
  updateInfo = info;
  els.updateState.textContent = `当前版本：${info.currentVersion || '--'}`;
  const latest = info.latestVersion || '--';
  const proxy = info.proxyLabel || 'GitHub 直连';
  if (info.hasUpdate) {
    els.updateSummary.innerHTML = `发现新版本 <strong>v${escapeHtml(latest)}</strong> · ${escapeHtml(proxy)} · ${info.elapsedMs || '-'}ms · ${escapeHtml(info.asset?.sizeText || '')}`;
    els.downloadUpdateBtn.disabled = !info.asset?.downloadUrl;
  } else {
    els.updateSummary.innerHTML = `已是最新版本 v${escapeHtml(info.currentVersion || latest)} · ${escapeHtml(proxy)} · ${info.elapsedMs || '-'}ms`;
    els.downloadUpdateBtn.disabled = true;
  }
  els.releaseNotes.innerHTML = renderMarkdownLite(info.body);
  showUpdateResult();

  // If speed test results were returned, cache them for mirror list
  if (info.speedTest) {
    updateOptions.lastSpeedResults = info.speedTest.results || [];
  }
}

async function saveUpdateSettings() {
  if (!window.managerApi.saveUpdateSettings) return;
  updateOptions.settings = await window.managerApi.saveUpdateSettings(updateOptions?.settings || {});
}

async function initUpdatePanel() {
  if (!window.managerApi.getUpdateOptions) return;
  updateOptions = await window.managerApi.getUpdateOptions();
  els.updateState.textContent = `当前版本：${updateOptions.version}`;
  updateOptions.lastSpeedResults = [];
}

function primaryPresetValue(provider) {
  if (!provider) return 'free';
  const kind = provider.kind || '';
  if (kind === 'anthropic') return 'anthropic';
  if (kind === 'deepseek' || kind === 'deepseek_chat' || kind === 'deepseek_compatible') return 'deepseek';
  const probe = `${provider.label || ''} ${provider.base_url || ''} ${provider.model || ''}`.toLowerCase();
  if (kind === 'openai_compatible' && (probe.includes('ollama') || probe.includes('translategemma') || probe.includes(':11434'))) return 'ollama';
  if (kind === 'openai_compatible' || kind === 'openai') return 'openai';
  if (kind === 'deepl') return 'deepl';
  if (kind === 'google_cloud' || kind === 'google_translate') return 'google-cloud';
  if (kind === 'microsoft' || kind === 'azure_translator') return 'microsoft';
  if (kind === 'baidu') return 'baidu';
  if (kind === 'youdao') return 'youdao';
  if (kind === 'tencent' || kind === 'tencent_cloud' || kind === 'tencent_tmt') return 'tencent';
  if (kind === 'aliyun' || kind === 'alibaba' || kind === 'alibaba_cloud' || kind === 'alimt') return 'aliyun';
  if (kind === 'volcengine' || kind === 'volc' || kind === 'volc_translate' || kind === 'huoshan') return 'volcengine';
  if (kind === 'libretranslate') return 'libretranslate';
  return 'openai';
}

function applyConfigObject(config, statusText = '') {
  els.targetLang.value = config.target_lang || 'zh-CN';
  els.overlayHotkey.value = config.overlay_hotkey || 'Ctrl+Shift+T';
  els.workers.value = config.workers ?? 8;
  els.queueLimit.value = config.queue_limit ?? 1000;
  els.cacheLimit.value = config.cache_limit ?? 1500;
  els.timeoutMs.value = config.timeout_ms ?? 5000;
  els.fontSize.value = config.font_size ?? 18;
  setFontSize(config.font_size ?? 18);
  setOverlayOpacity(config.overlay_opacity ?? 98);
  const primary = (config.providers || []).find((provider) => provider.enabled && !['mymemory', 'andeer'].includes(provider.kind));
  if (primary) {
    els.preset.value = primaryPresetValue(primary);
    const selected = PROVIDER_PRESETS[els.preset.value] || PROVIDER_PRESETS.openai;
    els.providerLabel.value = primary.label || selected.label;
    els.baseUrl.value = primary.base_url || selected.baseUrl;
    els.apiKey.value = primary.api_key || '';
    els.apiSecret.value = primary.api_secret || '';
    els.model.value = primary.model || selected.model;
    els.sourceLang.value = primary.source || primary.source_lang || 'auto';
  } else {
    els.preset.value = 'free';
  }
  els.enableFallbacks.checked = (config.providers || []).some((provider) => ['mymemory', 'andeer'].includes(provider.kind));
  els.preview.value = JSON.stringify(config, null, 2);
  syncProviderUi();
  if (statusText) setStatus(statusText);
}

function renderPresetList(selectedName = '') {
  const options = ['<option value="">选择预设...</option>'];
  for (const preset of configPresets) {
    const selected = preset.name === selectedName ? ' selected' : '';
    options.push(`<option value="${escapeHtml(preset.name)}"${selected}>${escapeHtml(preset.name)}</option>`);
  }
  els.configPreset.innerHTML = options.join('');
}

async function refreshPresets(selectedName = '') {
  configPresets = await window.managerApi.listPresets();
  renderPresetList(selectedName);
}

function syncProviderUi() {
  const selected = PROVIDER_PRESETS[els.preset.value] || PROVIDER_PRESETS.free;
  els.llmFields.style.display = selected.kind ? 'grid' : 'none';
  els.model.closest('.form-group').style.display = selected.needsModel ? 'flex' : 'none';
  els.apiSecret.closest('.form-group').style.display = selected.needsSecret ? 'flex' : 'none';
  els.apiSecret.placeholder = selected.secretLabel || '签名平台需要';
  els.model.closest('.form-group').querySelector('label').textContent = selected.modelLabel || '模型';
  els.apiKey.placeholder = selected.needsApiKey ? '必填' : '可选';
}

function updatePreview() {
  syncProviderUi();
  els.preview.value = JSON.stringify(buildConfig(), null, 2);
}

function applyPresetDefaults(force = false) {
  const selected = PROVIDER_PRESETS[els.preset.value] || PROVIDER_PRESETS.free;
  if (!selected.kind) {
    updatePreview();
    return;
  }
  if (force || !els.providerLabel.value.trim()) els.providerLabel.value = selected.label;
  if (force || !els.baseUrl.value.trim()) els.baseUrl.value = selected.baseUrl;
  if (force || !els.model.value.trim()) els.model.value = selected.model;
  if (force && selected.defaultWorkers) els.workers.value = selected.defaultWorkers;
  if (force && selected.defaultTimeoutMs) els.timeoutMs.value = selected.defaultTimeoutMs;
  if (!els.sourceLang.value.trim()) els.sourceLang.value = 'auto';
  updatePreview();
}

async function refreshState() {
  const state = await window.managerApi.state(currentGame, els.ets2Path.value.trim());
  els.installState.textContent = state.text;
  els.pluginDir.textContent = state.pluginDir ? `插件目录：${state.pluginDir}` : '';
}

async function loadInstalledConfig(silent = false) {
  try {
    const text = await window.managerApi.readConfig(currentGame, els.ets2Path.value.trim());
    if (!text) {
      if (!silent) setStatus('插件目录里还没有配置文件');
      return;
    }
    applyConfigObject(JSON.parse(text), '已读取已安装配置');
  } catch (error) {
    setStatus(`读取配置失败：${error.message}`);
  }
}

/* ==================== Install Panel ==================== */
els.detectBtn.addEventListener('click', async () => {
  els.ets2Path.value = await window.managerApi.detectPath(currentGame);
  setStatus(els.ets2Path.value ? `已自动识别 ${GAME_LABELS[currentGame]} 路径` : '未能自动识别，请手动选择');
  await refreshState();
  await loadInstalledConfig(true);
});

els.browseBtn.addEventListener('click', async () => {
  const selected = await window.managerApi.browsePath(currentGame);
  if (selected) {
    els.ets2Path.value = selected;
    await refreshState();
    await loadInstalledConfig(true);
  }
});

els.installBtn.addEventListener('click', async () => {
  try {
    await window.managerApi.installDll(currentGame, els.ets2Path.value.trim());
    setStatus('DLL 已安装/更新');
    await refreshState();
  } catch (error) {
    setStatus(`安装失败：${error.message}`);
  }
});

els.uninstallBtn.addEventListener('click', async () => {
  try {
    await window.managerApi.uninstallDll(currentGame, els.ets2Path.value.trim());
    setStatus('DLL 已卸载，配置已保留');
    await refreshState();
  } catch (error) {
    setStatus(`卸载失败：${error.message}`);
  }
});

/* ==================== Config Panel ==================== */
els.loadConfigBtn.addEventListener('click', () => loadInstalledConfig(false));
els.overlayPreviewBtn.addEventListener('click', async () => {
  try {
    await window.managerApi.previewOverlay();
    setStatus('已打开悬浮窗预览，可点击搜索框输入测试');
  } catch (error) {
    setStatus(`打开悬浮窗预览失败：${error.message}`);
  }
});
els.previewBtn.addEventListener('click', updatePreview);

els.configPreset.addEventListener('change', () => {
  els.presetName.value = els.configPreset.value;
});

els.applyPresetBtn.addEventListener('click', () => {
  const preset = configPresets.find((item) => item.name === els.configPreset.value);
  if (!preset) {
    setStatus('请选择要应用的配置预设');
    return;
  }
  applyConfigObject(preset.config, `已应用预设：${preset.name}`);
  els.presetName.value = preset.name;
});

els.savePresetBtn.addEventListener('click', async () => {
  try {
    const jsonText = els.preview.value;
    JSON.parse(jsonText);
    const name = els.presetName.value.trim() || els.configPreset.value.trim();
    if (!name) {
      setStatus('请先填写预设名称');
      return;
    }
    await window.managerApi.savePreset(name, jsonText);
    await refreshPresets(name);
    els.presetName.value = name;
    setStatus(`已保存配置预设：${name}`);
  } catch (error) {
    setStatus(`保存预设失败：${error.message}`);
  }
});

els.deletePresetBtn.addEventListener('click', async () => {
  try {
    const name = els.configPreset.value || els.presetName.value.trim();
    if (!name) {
      setStatus('请选择要删除的配置预设');
      return;
    }
    await window.managerApi.deletePreset(name);
    await refreshPresets();
    if (els.presetName.value === name) els.presetName.value = '';
    setStatus(`已删除配置预设：${name}`);
  } catch (error) {
    setStatus(`删除预设失败：${error.message}`);
  }
});

els.testConfigBtn.addEventListener('click', async () => {
  try {
    updatePreview();
    JSON.parse(els.preview.value);
    els.testConfigBtn.disabled = true;
    els.testConfigBtn.textContent = '测试中...';
    els.testResult.innerHTML = '<div class="test-summary">正在测试已启用 Provider...</div>';
    const result = await window.managerApi.testConfig(els.preview.value);
    renderTestResult(result);
    setStatus(result.ok ? `测试完成：${result.summary}` : `测试失败：${result.summary}`);
  } catch (error) {
    els.testResult.innerHTML = `<div class="test-summary fail">${escapeHtml(error.message)}</div>`;
    setStatus(`测试失败：${error.message}`);
  } finally {
    els.testConfigBtn.disabled = false;
    els.testConfigBtn.textContent = '测试配置';
  }
});

els.saveConfigBtn.addEventListener('click', async () => {
  try {
    JSON.parse(els.preview.value);
    els.saveConfigBtn.disabled = true;
    els.saveConfigBtn.textContent = '保存中...';
    await window.managerApi.writeConfig(currentGame, els.ets2Path.value.trim(), els.preview.value);
    setStatus('翻译配置已保存');
    await refreshState();
  } catch (error) {
    setStatus(`保存失败：${error.message}`);
  } finally {
    els.saveConfigBtn.disabled = false;
    els.saveConfigBtn.textContent = '保存配置';
  }
});

/* ==================== Update Panel ==================== */

// "检查更新" button → show mirror selection
els.checkUpdateBtn.addEventListener('click', async () => {
  showMirrorSelection();
  setStatus('请选择一个镜像源来检查更新');
});

// "直连检查" button → check update directly without proxy
els.useDirectBtn.addEventListener('click', async () => {
  try {
    selectMirror('direct');
    const settings = updateOptions?.settings || {};
    settings.proxyMode = 'manual';
    settings.proxyId = 'direct';
    await saveUpdateSettings();

    els.useDirectBtn.disabled = true;
    els.speedTestBtn.disabled = true;
    els.useSelectedMirrorBtn.disabled = true;
    els.useDirectBtn.textContent = '检查中...';
    setStatus('正在使用 GitHub 直连检查更新...');
    els.updateSummary.textContent = '正在读取 GitHub Release...';

    const result = await window.managerApi.checkUpdate(false);
    renderUpdateInfo(result);
    setStatus(result.hasUpdate ? `发现新版本 v${result.latestVersion}` : '已经是最新版本');
  } catch (error) {
    els.updateSummary.innerHTML = `<span class="error-text">检查更新失败：${escapeHtml(error.message)}</span>`;
    els.releaseNotes.innerHTML = '<p>直连检查失败，可以尝试使用镜像源。</p>';
    showUpdateResult();
    setStatus(`检查更新失败：${error.message}`);
  } finally {
    els.useDirectBtn.disabled = false;
    els.speedTestBtn.disabled = false;
    els.useSelectedMirrorBtn.disabled = false;
    els.useDirectBtn.textContent = '直连检查（不用代理）';
  }
});

// Speed test button
els.speedTestBtn.addEventListener('click', async () => {
  try {
    els.speedTestBtn.disabled = true;
    els.speedTestBtn.textContent = '测速中...';
    els.mirrorList.innerHTML = '<div class="mirror-empty">正在并发测试全部镜像，完成后按速度排序...</div>';
    const result = await window.managerApi.speedTestUpdateProxies();
    updateOptions.lastSpeedResults = result.results || [];
    if (result.fastest) {
      // Auto-select fastest
      selectedMirrorId = result.fastest.id;
    }
    renderMirrorList(updateOptions.lastSpeedResults);
    setStatus(result.fastest
      ? `测速完成：${result.fastest.label} 最快（已自动选中）`
      : '测速完成：没有可用代理');
  } catch (error) {
    els.mirrorList.innerHTML = `<div class="test-summary fail">${escapeHtml(error.message)}</div>`;
    setStatus(`测速失败：${error.message}`);
  } finally {
    els.speedTestBtn.disabled = false;
    els.speedTestBtn.textContent = '测速';
  }
});

// "使用选中镜像检查" button → actually check update
els.useSelectedMirrorBtn.addEventListener('click', async () => {
  if (!selectedMirrorId) {
    setStatus('请先在下方选择一个镜像源或点击测速');
    return;
  }
  try {
    // Save selected mirror as the current proxy
    const settings = updateOptions?.settings || {};
    if (selectedMirrorId === 'custom') {
      if (!settings.customProxyUrl) {
        setStatus('请先添加自定义镜像地址');
        return;
      }
      settings.proxyMode = 'custom';
    } else {
      settings.proxyMode = 'manual';
      settings.proxyId = selectedMirrorId;
    }
    await saveUpdateSettings();

    els.useSelectedMirrorBtn.disabled = true;
    els.useSelectedMirrorBtn.textContent = '检查中...';
    setStatus(`正在使用 ${mirrorLabel(selectedMirrorId, settings.customProxyUrl)} 检查更新...`);
    els.updateSummary.textContent = '正在读取 GitHub Release...';

    const result = await window.managerApi.checkUpdate(false);
    renderUpdateInfo(result);
    setStatus(result.hasUpdate ? `发现新版本 v${result.latestVersion}` : '已经是最新版本');
  } catch (error) {
    els.updateSummary.innerHTML = `<span class="error-text">检查更新失败：${escapeHtml(error.message)}</span>`;
    els.releaseNotes.innerHTML = '<p>检查失败，稍后可以换一个代理再试。</p>';
    showUpdateResult();
    setStatus(`检查更新失败：${error.message}`);
  } finally {
    els.useSelectedMirrorBtn.disabled = false;
    els.useSelectedMirrorBtn.textContent = '使用选中镜像检查';
  }
});

// Save custom mirror
els.saveMirrorBtn.addEventListener('click', async () => {
  try {
    const raw = els.mirrorUrl.value.trim();
    if (!raw) {
      setStatus('请先输入镜像地址');
      return;
    }
    const url = raw.startsWith('http') ? raw : `https://${raw}`;
    new URL(url);
    updateOptions.settings.customProxyUrl = url.replace(/\/+$/, '');
    await saveUpdateSettings();
    selectMirror('custom');
    renderMirrorList(updateOptions.lastSpeedResults || []);
    setStatus(`已保存自定义镜像：${mirrorLabel('custom', updateOptions.settings.customProxyUrl)}`);
  } catch (error) {
    setStatus(`镜像地址无效：${error.message}`);
  }
});

// Clear custom mirror
els.clearMirrorBtn.addEventListener('click', async () => {
  updateOptions.settings.customProxyUrl = '';
  updateOptions.settings.proxyMode = 'auto';
  updateOptions.settings.proxyId = '';
  await saveUpdateSettings();
  selectedMirrorId = null;
  renderMirrorList(updateOptions.lastSpeedResults || []);
  setStatus('已清除自定义镜像');
});

// Download update
els.downloadUpdateBtn.addEventListener('click', async () => {
  if (!updateInfo?.asset?.downloadUrl) {
    setStatus('没有可下载的更新安装包');
    return;
  }
  try {
    await saveUpdateSettings();
    els.downloadUpdateBtn.disabled = true;
    els.downloadUpdateBtn.textContent = '下载中...';
    els.updateProgress.hidden = false;
    els.updateProgressBar.style.width = '0%';
    els.updateProgressText.textContent = '正在准备下载...';

    const proxyId = selectedMirrorId || updateInfo.proxyId || '';
    const result = await window.managerApi.downloadUpdate(
      updateInfo.asset.downloadUrl,
      updateInfo.asset.name,
      proxyId
    );
    els.updateProgressBar.style.width = '100%';
    els.updateProgressText.textContent = `已下载 ${result.bytesText}，安装器已启动`;
    setStatus(`安装器已启动：${result.proxyLabel}`);
  } catch (error) {
    els.updateProgressText.textContent = `下载失败：${error.message}`;
    setStatus(`下载更新失败：${error.message}`);
    els.downloadUpdateBtn.disabled = false;
  } finally {
    els.downloadUpdateBtn.textContent = '下载并安装';
  }
});

if (window.managerApi.onUpdateDownloadProgress) {
  window.managerApi.onUpdateDownloadProgress((progress) => {
    els.updateProgress.hidden = false;
    const percent = progress.percent || 0;
    els.updateProgressBar.style.width = `${Math.max(0, Math.min(100, percent))}%`;
    const total = progress.totalText ? ` / ${progress.totalText}` : '';
    els.updateProgressText.textContent = `${progress.fileName} · ${progress.downloadedText}${total} · ${progress.proxyLabel}`;
  });
}

/* ==================== Global Input Listeners ==================== */
for (const input of [
  els.preset,
  els.targetLang,
  els.overlayHotkey,
  els.workers,
  els.queueLimit,
  els.cacheLimit,
  els.timeoutMs,
  els.fontSize,
  els.overlayOpacity,
  els.providerLabel,
  els.baseUrl,
  els.apiKey,
  els.apiSecret,
  els.model,
  els.sourceLang,
  els.enableFallbacks
]) {
  input.addEventListener('input', updatePreview);
  input.addEventListener('change', updatePreview);
}

els.preset.addEventListener('change', () => applyPresetDefaults(true));

document.querySelectorAll('.game-tab').forEach((tab) => {
  tab.addEventListener('click', async () => {
    currentGame = tab.dataset.game || 'ets2';
    document.querySelectorAll('.game-tab').forEach((item) => {
      item.classList.toggle('active', item === tab);
    });
    els.gamePathLabel.textContent = `${GAME_LABELS[currentGame]} 安装目录`;
    els.ets2Path.value = await window.managerApi.detectPath(currentGame);
    setStatus(els.ets2Path.value ? `已切换到 ${GAME_LABELS[currentGame]}` : `${GAME_LABELS[currentGame]} 路径未设置`);
    await refreshState();
    await loadInstalledConfig(true);
  });
});

/* ==================== Font Size Control ==================== */
function setFontSize(size) {
  size = Math.max(12, Math.min(28, size));
  els.fontSize.value = size;
  els.fontSizeValue.textContent = size;
  if (els.fontPreview) {
    els.fontPreview.style.fontSize = `${size}px`;
  }
  document.querySelectorAll('.preset-btn').forEach(btn => {
    btn.classList.toggle('active', Number(btn.dataset.size) === size);
  });
  updatePreview();
}

function setOverlayOpacity(value) {
  const parsed = Number.parseInt(value, 10);
  const opacity = Math.max(0, Math.min(100, Number.isFinite(parsed) ? parsed : 98));
  els.overlayOpacity.value = opacity;
  els.overlayOpacityValue.textContent = opacity;
  updatePreview();
}

els.fontSizeMinus.addEventListener('click', () => {
  setFontSize(numberValue(els.fontSize, 18) - 2);
});

els.fontSizePlus.addEventListener('click', () => {
  setFontSize(numberValue(els.fontSize, 18) + 2);
});

els.overlayOpacity.addEventListener('input', () => {
  setOverlayOpacity(els.overlayOpacity.value);
});

document.querySelectorAll('.preset-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    setFontSize(Number(btn.dataset.size));
  });
});

const copyPreviewBtn = document.querySelector('#copyPreviewBtn');
if (copyPreviewBtn) {
  copyPreviewBtn.addEventListener('click', async () => {
    try {
      const text = els.preview.value;
      await navigator.clipboard.writeText(text);
      const span = copyPreviewBtn.querySelector('span');
      const originalText = span.textContent;
      span.textContent = '已复制!';
      copyPreviewBtn.classList.add('copied');
      setTimeout(() => {
        span.textContent = originalText;
        copyPreviewBtn.classList.remove('copied');
      }, 2000);
    } catch (err) {
      setStatus(`复制失败: ${err.message}`);
    }
  });
}

/* ==================== Init ==================== */
(async function init() {
  els.ets2Path.value = await window.managerApi.detectPath(currentGame);
  await initUpdatePanel();
  await refreshPresets();
  setFontSize(numberValue(els.fontSize, 18));
  setOverlayOpacity(numberValue(els.overlayOpacity, 98));
  applyPresetDefaults(true);
  await refreshState();
  await loadInstalledConfig(true);
})();
