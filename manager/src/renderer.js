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
  previewBtn: document.querySelector('#previewBtn'),
  testConfigBtn: document.querySelector('#testConfigBtn'),
  saveConfigBtn: document.querySelector('#saveConfigBtn'),
  testResult: document.querySelector('#testResult'),
  preview: document.querySelector('#preview')
};

let currentGame = 'ets2';
let configPresets = [];
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
    timeout_ms: Math.max(1500, Math.min(6000, numberValue(els.timeoutMs, 5000))),
    font_size: Math.max(12, Math.min(28, numberValue(els.fontSize, 18))),
    overlay_opacity: Math.max(35, Math.min(100, numberValue(els.overlayOpacity, 98))),
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
      base_url: els.baseUrl.value.trim(),
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

function primaryPresetValue(provider) {
  if (!provider) return 'free';
  const kind = provider.kind || '';
  if (kind === 'anthropic') return 'anthropic';
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

els.loadConfigBtn.addEventListener('click', () => loadInstalledConfig(false));
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
    await window.managerApi.writeConfig(currentGame, els.ets2Path.value.trim(), els.preview.value);
    setStatus('翻译配置已保存');
    await refreshState();
  } catch (error) {
    setStatus(`保存失败：${error.message}`);
  }
});

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

// --- 字体大小控件 ---
function setFontSize(size) {
  size = Math.max(12, Math.min(28, size));
  els.fontSize.value = size;
  els.fontSizeValue.textContent = size;
  if (els.fontPreview) {
    els.fontPreview.style.fontSize = `${size}px`;
  }
  // 高亮对应预设按钮
  document.querySelectorAll('.preset-btn').forEach(btn => {
    btn.classList.toggle('active', Number(btn.dataset.size) === size);
  });
  updatePreview();
}

function setOverlayOpacity(value) {
  const opacity = Math.max(35, Math.min(100, Number.parseInt(value, 10) || 98));
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

(async function init() {
  els.ets2Path.value = await window.managerApi.detectPath(currentGame);
  await refreshPresets();
  setFontSize(numberValue(els.fontSize, 18));
  setOverlayOpacity(numberValue(els.overlayOpacity, 98));
  applyPresetDefaults(true);
  await refreshState();
  await loadInstalledConfig(true);
})();
