/* 由 web/embed_pages.py 生成：各 HTML 页面的 PROGMEM 字符串。不要手改。 */
#pragma once

const char LIGHT_PAGE[] PROGMEM = R"HTML(<!-- 开关页：连上家里 WiFi 后点按钮通断继电器。 -->
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="mobile-web-app-capable" content="yes">
  <meta name="theme-color" content="#101216">
  <title>灯光</title>
  <style>
    :root { color-scheme: dark; }
    * { box-sizing: border-box; }
    body {
      margin: 0; min-height: 100dvh;
      display: flex; flex-direction: column; align-items: center; justify-content: center;
      font-family: system-ui, sans-serif; background: #101216; color: #e8eaed;
      gap: 28px; padding: 24px;
    }
    h1 { font-size: 1.1rem; font-weight: 600; letter-spacing: .12em; color: #9aa0a6; margin: 0; }
    #btn {
      width: min(64vw, 220px); aspect-ratio: 1; border-radius: 50%;
      border: 0; cursor: pointer; color: #fff; font-size: 1.6rem; font-weight: 700;
      background: #3c4043; box-shadow: 0 10px 30px rgba(0,0,0,.35);
      transition: transform .12s, background .2s, box-shadow .2s;
    }
    #btn.on {
      background: #e8b931; color: #1a1403;
      box-shadow: 0 0 0 10px rgba(232,185,49,.18), 0 10px 30px rgba(232,185,49,.25);
    }
    #btn:active { transform: scale(.96); }
    #state { margin: 0; color: #9aa0a6; font-size: .95rem; }
    form { margin: 0; }
    .link {
      background: none; border: 0; color: #9aa0a6; font-size: .85rem; text-decoration: underline;
    }
  </style>
</head>
<body>
  <h1>灯光开关</h1>
  <button id="btn" type="button">关</button>
  <p id="state">正在连接…</p>
  <form action="/forget" method="get">
    <input type="hidden" name="confirm" value="1">
    <button class="link" type="submit">更换 WiFi</button>
  </form>
  <script>
    const btn = document.getElementById('btn');
    const state = document.getElementById('state');
    function render(on) {
      btn.className = on ? 'on' : '';
      btn.textContent = on ? '开' : '关';
      state.textContent = on ? '灯已打开' : '灯已关闭';
    }
    async function refresh() {
      const r = await fetch('/api/status');
      const j = await r.json();
      render(j.on);
    }
    btn.onclick = async () => {
      const r = await fetch('/api/toggle');
      const j = await r.json();
      render(j.on);
    };
    refresh();
    setInterval(refresh, 4000);
  </script>
</body>
</html>
)HTML";

const char SAVED_PAGE[] PROGMEM = R"HTML(<!-- 保存成功提示：凭据已写入，板子即将重启去连网。 -->
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>已保存</title>
  <style>
    body {
      background: #101216;
      color: #e8eaed;
      font-family: system-ui, sans-serif;
      padding: 40px;
      text-align: center;
    }
  </style>
</head>
<body>
  <p>已保存，板子正在重启并连接 WiFi。</p>
  <p>请改连家里的 WiFi，看点阵上滚动的地址。</p>
</body>
</html>
)HTML";

const char SETUP_PAGE[] PROGMEM = R"HTML(<!-- 配网页：手机连 Light-Setup 后填家里的 2.4GHz WiFi。 -->
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
  <meta name="theme-color" content="#101216">
  <title>配置灯光</title>
  <style>
    :root { color-scheme: dark; }
    * { box-sizing: border-box; }
    body {
      margin: 0; min-height: 100dvh; font-family: system-ui, sans-serif;
      background: #101216; color: #e8eaed; padding: 28px 20px;
      display: flex; flex-direction: column; align-items: center;
    }
    h1 { font-size: 1.1rem; letter-spacing: .08em; color: #9aa0a6; }
    form, p { width: min(100%, 360px); }
    label { display: block; margin: 16px 0 8px; color: #9aa0a6; font-size: .9rem; }
    input {
      width: 100%; padding: 14px; border: 0; border-radius: 12px;
      background: #2a2e33; color: #fff; font-size: 1rem;
    }
    button {
      width: 100%; margin-top: 24px; padding: 14px; border: 0; border-radius: 12px;
      background: #e8b931; color: #1a1403; font-size: 1rem; font-weight: 700;
    }
    p { color: #9aa0a6; line-height: 1.5; font-size: .9rem; }
  </style>
</head>
<body>
  <h1>配置 WiFi</h1>
  <p>只支持 2.4GHz。保存后板子会重启，点阵滚动 IP 后即可开关灯。</p>
  <form action="/save" method="get">
    <label>WiFi 名称</label>
    <input name="ssid" autocomplete="off" required>
    <label>密码（开放网络可留空）</label>
    <input name="pass" type="password">
    <button type="submit">保存并连接</button>
  </form>
</body>
</html>
)HTML";

