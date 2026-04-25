const chatWrapper = document.getElementById('chat-wrapper');
const userInput = document.getElementById('user-input');
const sendBtn = document.getElementById('send-btn');
const moodBadge = document.getElementById('mood-badge');

let messages = [];
let audioPeak = 0;
let lastJumpTime = 0;

function appendMessage(role, text) {
    const msgDiv = document.createElement('div');
    msgDiv.className = `message ${role}`;
    const bubble = document.createElement('div');
    bubble.className = 'bubble';
    bubble.innerHTML = text; 
    msgDiv.appendChild(bubble);
    chatWrapper.appendChild(msgDiv);
    chatWrapper.scrollTop = chatWrapper.scrollHeight;
    return bubble;
}

function updateMood(tag) {
    moodBadge.textContent = tag;
    moodBadge.style.background = 'transparent';
    moodBadge.style.color = '#fff';
    moodBadge.style.borderColor = '#222';
    
    if (tag === 'NORMAL') {
        moodBadge.style.color = '#666';
    } else {
        moodBadge.style.color = '#fff';
        moodBadge.style.borderColor = '#fff';
    }
}

async function sendMessage() {
    const text = userInput.value.trim();
    if (!text) return;
    
    appendMessage('user', text);
    messages.push({ role: "user", content: text });
    userInput.value = '';
    
    // Create assistant placeholder
    const bubble = appendMessage('assistant', '<span class="blink-cursor"></span>');
    
    // Disable input while thinking
    userInput.disabled = true;
    sendBtn.disabled = true;
    userInput.placeholder = "AI is thinking...";
    
    try {
        const response = await fetch('/chat', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ messages })
        });
        
        const reader = response.body.getReader();
        const decoder = new TextDecoder("utf-8");
        let assistantReply = "";
        let rawReply = "";
        let buffer = "";
        
        while (true) {
            const { value, done } = await reader.read();
            if (done) break;
            
            buffer += decoder.decode(value, { stream: true });
            
            let boundary;
            // Handle both \n\n and \r\n\r\n just in case
            while (buffer.includes('\n\n')) {
                boundary = buffer.indexOf('\n\n');
                let message = buffer.slice(0, boundary).trim();
                buffer = buffer.slice(boundary + 2);
                
                if (message.startsWith('data: ')) {
                    const dataStr = message.substring(6);
                    try {
                        const data = JSON.parse(dataStr);
                        console.log("Received data:", data);
                        if (data.type === 'tag') {
                            updateMood(data.content);
                            rawReply += `[${data.content}]`;
                        } else if (data.type === 'text') {
                            assistantReply += data.content;
                            rawReply += data.content;
                            bubble.innerHTML = assistantReply + '<span class="blink-cursor"></span>';
                            chatWrapper.scrollTop = chatWrapper.scrollHeight;
                        } else if (data.type === 'done') {
                            // Don't reset mood — let idle loop handle it
                        } else if (data.type === 'error') {
                            bubble.innerHTML = "<em>Error: " + data.content + "</em>";
                        }
                    } catch (e) {
                         // ignore incomplete json
                    }
                }
            }
        }
        
        // Finalize UI
        bubble.innerHTML = assistantReply;
        if (rawReply) {
             messages.push({ role: "assistant", content: rawReply });
        }
        
    } catch (error) {
        bubble.innerHTML = "<em>Error connecting to AI. Make sure Ollama is running!</em>";
    } finally {
        // Re-enable input
        userInput.disabled = false;
        sendBtn.disabled = false;
        userInput.placeholder = "Say something...";
        userInput.focus();
    }
}

sendBtn.addEventListener('click', sendMessage);
userInput.addEventListener('keypress', (e) => {
    if (e.key === 'Enter') sendMessage();
});

function control(action, params = {}) {
    const body = typeof params === 'object' ? { action, ...params } : { action, value: params };
    fetch('/control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
    });
}

function feed() {
    fetch('/feed', { method: 'POST' });
    // Local preview
    currentMode = 'EYES';
    currentMoodTag = 'FEAST';
    updateMood('HAPPY'); 
    setTimeout(() => { currentMoodTag = 'NORMAL'; updateMood('NORMAL'); }, 4000);
}

let configState = { flipH: false, flipV: false };
function toggleConfig(key) {
    configState[key] = !configState[key];
    control('config', configState);
}

function sendShout() {
    const msg = document.getElementById('shout-input').value;
    if (!msg) return;
    control('shout', { msg, speed: 50, pause: 4000 });
    document.getElementById('shout-input').value = '';
}

function showStats() {
    fetch('/show_stats');
}

async function triggerStat(name) {
    try {
        const res = await fetch(`/show_stat/${name}`);
        const data = await res.json();
        if (data.text) {
            appendMessage('assistant', `[SYSTEM] ${name.toUpperCase()}: ${data.text}`);
        }
    } catch (e) {
        console.error("Stat trigger failed", e);
    }
}

// --- Live mood polling ---
const BAR_COLORS = {
    HAPPY:'#fff', SAD:'#aaa', ANGRY:'#fff', NAUGHTY:'#fff',
    SUSPICIOUS:'#888', JEALOUS:'#888', ANNOYED:'#888', SICK:'#444',
    SCARE:'#fff', BORED:'#444', SLEEP:'#222', STARS:'#fff',
    DANCE:'#fff', SING:'#fff', NORMAL:'#666'
};

setInterval(async () => {
    try {
        const res = await fetch('/current_mood');
        const data = await res.json();
        if (data.mood) {
            updateMood(data.mood);
            const sm = document.getElementById('stat-mood');
            const si = document.getElementById('stat-id');
            const st = document.getElementById('stat-idle');
            const ss = document.getElementById('stat-state');
            if (sm) sm.textContent = data.mood;
            if (si) si.textContent = data.mood_id;
            if (st) {
                const s = data.idle_secs;
                st.textContent = s >= 60 ? Math.floor(s/60) + 'm ' + (s%60) + 's' : s + 's';
            }
            if (ss) ss.textContent = data.idle_state;

            // Render emotion bars
            if (data.stats) {
                const container = document.getElementById('bars-container');
                if (container) {
                    const maxVal = Math.max(1, ...Object.values(data.stats));
                    let html = '';
                    const sorted = Object.entries(data.stats).sort((a,b) => b[1] - a[1]);
                    for (const [name, count] of sorted) {
                        if (count === 0) continue;
                        const pct = Math.round((count / maxVal) * 100);
                        const color = BAR_COLORS[name] || '#8b5cf6';
                        html += `<span style="color:#ccc;text-align:right">${name}</span>`;
                        html += `<div style="background:rgba(255,255,255,0.08);border-radius:4px;height:14px;overflow:hidden">`;
                        html += `<div style="width:${pct}%;height:100%;background:${color};border-radius:4px;transition:width 0.5s"></div></div>`;
                        html += `<span style="color:#888">${count}</span>`;
                    }
                    container.innerHTML = html;
                }
            }
        }
    } catch (e) { /* ignore */ }
}, 3000);

// ─── Proactive Notifications ─────────────────────
const toastContainer = document.getElementById('toast-container');
const notifBell = document.getElementById('notif-bell');
let notifEnabled = true;

function showToast(notif) {
    const el = document.createElement('div');
    el.className = 'toast';
    el.innerHTML = `
        <div class="toast-header">
            <span class="toast-mood">🤖 ${notif.mood}</span>
            <span class="toast-time">${notif.timestamp}</span>
        </div>
        <div class="toast-body">${notif.message}</div>
        <div class="toast-hint">click to reply</div>
    `;

    // Click → seed chat input with EmoBot's message
    el.addEventListener('click', () => {
        const input = document.getElementById('user-input');
        // Seed the conversation — add EmoBot's proactive message as context
        appendMessage('assistant', notif.message);
        messages.push({ role: "assistant", content: `[${notif.mood}] ${notif.message}` });
        input.focus();
        // Dismiss toast
        el.classList.add('toast-exit');
        setTimeout(() => el.remove(), 400);
    });

    toastContainer.appendChild(el);

    // Auto-dismiss after 8s
    setTimeout(() => {
        if (el.parentNode) {
            el.classList.add('toast-exit');
            setTimeout(() => el.remove(), 400);
        }
    }, 8000);
}

// Poll for notifications every 5 seconds
setInterval(async () => {
    try {
        // First peek to update bell indicator
        const peek = await fetch('/notifications/peek');
        const peekData = await peek.json();

        if (peekData.has_notification) {
            notifBell.classList.add('has-notif');

            // Fetch and show the notification (clears it server-side)
            const res = await fetch('/notifications');
            const data = await res.json();
            if (data.notification) {
                showToast(data.notification);
                updateMood(data.notification.mood);
            }
        } else {
            notifBell.classList.remove('has-notif');
        }
    } catch (e) { /* ignore */ }
}, 5000);

// Manual bell click — fetch notification
async function fetchAndShowNotification() {
    try {
        const res = await fetch('/notifications');
        const data = await res.json();
        if (data.notification) {
            showToast(data.notification);
        } else {
            // No pending — show brief "no new" toast
            const el = document.createElement('div');
            el.className = 'toast';
            el.innerHTML = `
                <div class="toast-body" style="color:#555">No new notifications</div>
            `;
            toastContainer.appendChild(el);
            setTimeout(() => {
                el.classList.add('toast-exit');
                setTimeout(() => el.remove(), 400);
            }, 2000);
        }
        notifBell.classList.remove('has-notif');
    } catch (e) { /* ignore */ }
}

// Toggle notifications on/off
async function toggleNotifications() {
    notifEnabled = !notifEnabled;
    const toggle = document.getElementById('notif-toggle');
    toggle.classList.toggle('active', notifEnabled);
    try {
        await fetch('/notifications/settings', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ enabled: notifEnabled })
        });
    } catch (e) { /* ignore */ }
}

// Load initial notification settings
(async function loadNotifSettings() {
    try {
        const res = await fetch('/notifications/settings');
        const data = await res.json();
        notifEnabled = data.enabled;
        const toggle = document.getElementById('notif-toggle');
        if (toggle) toggle.classList.toggle('active', notifEnabled);
        const label = document.getElementById('quiet-hours-label');
        if (label) {
            const qs = String(data.quiet_start).padStart(2, '0');
            const qe = String(data.quiet_end).padStart(2, '0');
            label.textContent = `${qs}:00 – ${qe}:00`;
        }
    } catch (e) { /* ignore */ }
})();
// ─── Virtual Matrix Engine (1:1 C++ Port) ──────────
const matrix = document.getElementById('virtual-matrix');
const LEDS = [];
let currentMoodTag = 'NORMAL';
let currentMode = 'EYES';
let shoutText = '', shoutPos = 32;
let animationFrame = null, lastUpdate = 0;

// Eye drift state (matches C++ roboEyeX/Y/MoveTimer)
let eyeX = 0, eyeY = 0, moveTimer = 0;
// Blink state
let blinkPhase = 0, blinkCounter = 0;

const MOOD_IDS = {
    'NORMAL':0,'HAPPY':1,'ANGRY':2,'SAD':3,'SUSPICIOUS':4,'JEALOUS':5,
    'NAUGHTY':6,'WEATHER_SUN':7,'WEATHER_RAIN':8,'WEATHER_SNOW':9,
    'SICK':10,'SCARE':11,'ANNOYED':12,'SLEEP':13,'BORED':14,'GAME':15,
    'STARS':16,'DANCE':17,'SING':18,'DIZZY':19,'LOVE':20,'SAD_CRY':21,
    'THINK':22,'SNEEZE':23,'WINK':24,'HAPPY_CRY':25,'EXCLAIM':26,'QUESTION':27,
    'FEAST':28
};

function initMatrix() {
    matrix.innerHTML = '';
    LEDS.length = 0;
    for (let i = 0; i < 256; i++) {
        const d = document.createElement('div');
        d.className = 'led';
        matrix.appendChild(d);
        LEDS.push(d);
    }
}

function px(r, c, on) {
    r = Math.round(r); c = Math.round(c);
    if (r < 0 || r >= 8 || c < 0 || c >= 32) return;
    if (on) LEDS[r * 32 + c].classList.add('on');
}

function drawRoboEye(startC, mood, blink, isLeft) {
    const t = Date.now();
    const breathe = Math.sin(t / 1500) * 0.3;
    const jump = Math.min(audioPeak * 2.0, 1.0); // clamped so eyes stay visible
    
    let h = 5 + breathe; 
    let w = 4;
    let rOff = 1 - (breathe / 2);
    let cOff = 1;
    let fX = eyeX;
    let fY = eyeY;
    let shape = Array.from({length:8}, () => Array(6).fill(false));

    // Mood geometry (from C++ drawRoboEye)
    if (mood === 6) { rOff = isLeft ? 0 : 2; }
    else if (mood === 5) { if (isLeft) cOff = 2; else { h = 3; rOff = 3; } }
    else if (mood === 10) { h = 3; rOff = 2; cOff = 1 + (Math.floor(t/50)%3) - 1; }
    else if (mood === 12) {
        const p = Math.floor(t/150)%8;
        const pos = [[1,1],[0,1],[0,2],[0,2],[1,2],[2,2],[2,1],[2,0]];
        rOff = pos[p][0]; cOff = pos[p][1];
    } else if (mood === 11) { h = 6; w = 5; rOff = 0; cOff = 0; }
    else if (mood === 13) { h = 1; w = 5; rOff = 3; cOff = 0; }
    else if (mood === 14) { h = 5; w = 5; rOff = 1; cOff = 0; }

    // Fill base rectangle (skip weather 7-9)
    if (mood < 7 || mood >= 10) {
        for (let r=0; r<h; r++) for (let c=0; c<w; c++) {
            let rr = Math.round(r + rOff), cc = Math.round(c + cOff);
            if (rr >= 0 && rr < 8 && cc >= 0 && cc < 6) shape[rr][cc] = true;
        }
    } else {
        // Weather icons
        if (mood === 7) { // SUN
            [[1,2],[2,1],[2,2],[2,3],[3,0],[3,1],[3,2],[3,3],[3,4],[4,1],[4,2],[4,3],[5,2]]
                .forEach(([r,c]) => shape[r][c] = true);
        } else if (mood === 8) { // RAIN
            [[2,1],[2,2],[2,3],[3,0],[3,1],[3,2],[3,3],[3,4]]
                .forEach(([r,c]) => shape[r][c] = true);
            if (Math.floor(t/150)%2===0) { shape[5][0]=true; shape[6][2]=true; shape[5][4]=true; }
            else { shape[6][0]=true; shape[5][2]=true; shape[6][4]=true; }
        } else if (mood === 9) { // SNOW
            [[1,2],[2,1],[2,2],[2,3],[3,0],[3,2],[3,4],[4,1],[4,2],[4,3],[5,2]]
                .forEach(([r,c]) => shape[r][c] = true);
        }
    }

    // Mood carving (from C++)
    for (let r=0;r<8;r++) for (let c=0;c<6;c++) {
        if (!shape[r][c]) continue;
        if (mood===1) { // HAPPY — arch
            if (r >= rOff+3) shape[r][c] = false;
            if (r >= rOff+1 && c > cOff && c < cOff+w-1) shape[r][c] = false;
        } else if (mood===2) { // ANGRY — brow
            if (isLeft) {
                if (r===rOff && c>=cOff+2) shape[r][c] = false;
                if (r===rOff+1 && c===cOff+3) shape[r][c] = false;
            } else {
                if (r===rOff && c<=cOff+1) shape[r][c] = false;
                if (r===rOff+1 && c===cOff) shape[r][c] = false;
            }
        } else if (mood===3) { // SAD
            if (isLeft) { if (r===rOff && c<=cOff+1) shape[r][c] = false; }
            else { if (r===rOff && c>=cOff+2) shape[r][c] = false; }
            if (r >= rOff+4) shape[r][c] = false;
        } else if (mood===4) { // SUSPICIOUS
            if (r <= rOff+1) shape[r][c] = false;
        } else if (mood===11) { // SCARE
            if (r===0 && (c===0||c===2||c===4)) shape[r][c] = false;
            if (r===5 && (c===1||c===3)) shape[r][c] = false;
            if (Math.floor(t/50)%2===0 && r===2 && c===2) shape[r][c] = false;
        } else if (mood===14) { // BORED
            if (r <= rOff+1) shape[r][c] = false;
            if (Math.floor(t/2000)%2===0 && r <= rOff+2) shape[r][c] = false;
        }
    }

    // Blink phase
    if (mood < 7 || mood >= 10) {
        if (blink===1||blink===3) {
            for (let r=0;r<8;r++) for (let c=0;c<6;c++)
                if (r<=rOff || r>=rOff+h-1) shape[r][c] = false;
        } else if (blink===2) {
            for (let r=0;r<8;r++) for (let c=0;c<6;c++)
                if (r !== rOff+Math.floor(h/2)) shape[r][c] = false;
        }
    }

    // Combine offsets
    fX += 0; fY += 0; // fX and fY already initialized to eyeX/eyeY

    // SLEEP: ZZZ scroll
    if (mood===13) {
        blink = 2;
        if (Math.floor(t/30000)%2===1 && isLeft) {
            for (let r=0;r<8;r++) for (let c=0;c<6;c++) shape[r][c]=false;
            const sx = 32 - (Math.floor(t/150)%64);
            const drawZ = (x,y,big) => {
                const s = big?5:3;
                for (let i=0;i<s;i++) { px(y,x+i,true); px(y+s-1,x+i,true); px(y+s-1-i,x+i,true); }
            };
            drawZ(sx,4,false); drawZ(sx+6,3,false); drawZ(sx+12,4,false);
            drawZ(sx+20,2,true); drawZ(sx+28,2,true); drawZ(sx+38,1,true);
        } else { fY += Math.round(Math.sin(t/1500)*1.5); }
    }

    // DANCE: audio-reactive in-place animation
    if (mood===17) {
        // Horizontal sway driven by audio intensity
        const sway = Math.sin(t / (300 - audioPeak * 150)) * (2 + audioPeak * 3);
        fX += Math.round(sway);
        // Squish: alternate tall/short with beat
        if (audioPeak > 0.1) {
            h = (Math.floor(t / 150) % 2 === 0) ? 4 : 6;
            rOff = (h === 6) ? 0 : 2;
        }
        // Width pulse on strong beats
        if (audioPeak > 0.3) { w = 5; cOff = 0; }
    }

    // SING: floating notes (hide eyes)
    if (mood===18) {
        for (let r=0;r<8;r++) for (let c=0;c<6;c++) shape[r][c]=false;
        if (isLeft) {
            for (let i=0;i<2;i++) {
                const nx = 28 - (Math.floor(t/(250+i*50))%24);
                const ny = 2 + Math.round(Math.sin((t+i*1000)/400)*2);
                if (i===0) { px(ny,nx,true); px(ny+1,nx,true); px(ny+2,nx,true); px(ny,nx+1,true); }
                else { px(ny,nx,true); px(ny,nx+2,true); px(ny+1,nx,true); px(ny+1,nx+2,true); px(ny,nx+1,true); }
            }
        }
    }

    // STARS: twinkling (hide eyes)
    if (mood===16) {
        for (let r=0;r<8;r++) for (let c=0;c<6;c++) shape[r][c]=false;
        if (isLeft) {
            for (let i=0;i<6;i++) {
                const sx = (i*137+Math.floor(t/100))%32;
                const sy = (i*257+Math.floor(t/150))%8;
                if (Math.floor(t/(200+i*50))%2===0) px(sy,sx,true);
            }
        }
    }

    // BORED: organic scan
    if (mood===14) { fX = Math.round(Math.sin(t/3000)*5+Math.cos(t/1500)*2); }

    // NAUGHTY: breathe
    if (mood===6) { if (Math.sin(t/800)*0.2+0.9 < 0.9) fY += 1; }

    // ─── Extra Expressions ───────────────────

    if (mood===19) { // DIZZY — spiral in each eye
        for (let r=0;r<8;r++) for (let c=0;c<6;c++) shape[r][c]=false;
        const ang = t / 400;
        // Draw spiral: 8 points along expanding radius
        for (let i=0;i<8;i++) {
            const a = ang + i*0.8;
            const radius = 0.3 + i*0.35;
            const sr = Math.round(3 + Math.sin(a)*radius);
            const sc = Math.round(2.5 + Math.cos(a)*radius);
            if (sr>=0&&sr<8&&sc>=0&&sc<6) shape[sr][sc] = true;
        }
    }

    if (mood===20) { // LOVE — pulsing heart eyes
        for (let r=0;r<8;r++) for (let c=0;c<6;c++) shape[r][c]=false;
        // Heart shape in 6x5 grid, pulse scale
        const pulse = 1 + Math.sin(t/400)*0.25;
        const heart = [[0,1],[0,3],[1,0],[1,1],[1,2],[1,3],[1,4],[2,0],[2,1],[2,2],[2,3],[2,4],[3,1],[3,2],[3,3],[4,2]];
        heart.forEach(([r,c]) => {
            const pr = Math.round((r-2)*pulse+2), pc = Math.round((c-2)*pulse+2);
            if (pr>=0&&pr<8&&pc>=0&&pc<6) shape[pr][pc] = true;
        });
    }

    if (mood===21) { // SAD_CRY — sad eyes + tears
        for (let r=0;r<8;r++) for (let c=0;c<6;c++) shape[r][c]=false;
        for (let r=0;r<4;r++) for (let c=0;c<w;c++) {
            let rr = Math.round(r + rOff), cc = Math.round(c + cOff);
            if (rr >= 0 && rr < 8 && cc >= 0 && cc < 6) shape[rr][cc] = true;
        }
        if (Math.round(rOff)>=0 && Math.round(rOff)<8) {
            if (isLeft) { 
                if(Math.round(cOff)>=0&&Math.round(cOff)<6) shape[Math.round(rOff)][Math.round(cOff)]=false; 
                if(Math.round(cOff+1)>=0&&Math.round(cOff+1)<6) shape[Math.round(rOff)][Math.round(cOff+1)]=false; 
            } else { 
                if(Math.round(cOff+2)>=0&&Math.round(cOff+2)<6) shape[Math.round(rOff)][Math.round(cOff+2)]=false; 
                if(Math.round(cOff+3)>=0&&Math.round(cOff+3)<6) shape[Math.round(rOff)][Math.round(cOff+3)]=false; 
            }
        }
        // Tear drops
        const td = Math.floor(t/200)%4;
        px(5+td, startC+2+fX, true);
        if (td>0) px(4+td, startC+3+fX, true);
    }

    if (mood===25) { // HAPPY_CRY — happy arch eyes + tears
        for (let r=0;r<8;r++) for (let c=0;c<6;c++) shape[r][c]=false;
        // Happy arch
        const pts = [[0,0],[0,w-1],[1,0],[1,w-1],[2,0],[2,w-1]];
        for (let c=0; c<w; c++) pts.push([0,c]);
        pts.forEach(([r,c]) => {
            let rr = Math.round(r + rOff), cc = Math.round(c + cOff);
            if (rr >= 0 && rr < 8 && cc >= 0 && cc < 6) shape[rr][cc] = true;
        });
        // Tears of joy
        const ht = Math.floor(t/250)%4;
        px(rOff+3+ht, startC+1+fX, true);
        px(rOff+3+((ht+2)%4), startC+4+fX, true);
    }

    if (mood===22) { // THINK — thought bubble only (no eyes)
        for (let r=0;r<8;r++) for (let c=0;c<6;c++) shape[r][c]=false;
        if (isLeft) {
            // Small rising dots on left side
            const dots = Math.floor(t/800)%4;
            if (dots>=1) px(6, startC+3, true);
            if (dots>=2) px(4, startC+4, true);
            if (dots>=3) px(2, startC+5, true);
        } else {
            // Growing thought cloud on right side
            const dots = Math.floor(t/800)%4;
            if (dots>=1) { px(1, startC+0, true); px(1, startC+1, true); }
            if (dots>=2) { px(0, startC+0, true); px(0, startC+1, true); px(0, startC+2, true); px(1, startC+2, true); }
            if (dots>=3) { px(0, startC+3, true); px(1, startC+3, true); px(2, startC+0, true); }
        }
    }

    if (mood===23) { // SNEEZE
        for (let r=0;r<8;r++) for (let c=0;c<6;c++) shape[r][c]=false;
        if ((t%2000)<1500) {
            const sh = (Math.floor(t/50)%2===0)?1:-1;
            for (let r=0;r<3;r++) for (let c=0;c<4;c++) { const nc=2+c+sh; if(nc>=0&&nc<6) shape[r+3][nc]=true; }
        } else {
            for (let i=0;i<8;i++) { const rr=Math.floor(Math.random()*8), rc=Math.floor(Math.random()*6); shape[rr][rc]=true; }
        }
    }

    if (mood===24 && !isLeft) { // WINK — right eye closed
        for (let r=0;r<8;r++) for (let c=0;c<6;c++) shape[r][c]=false;
        for (let c=0;c<4;c++) shape[4][c+1]=true;
    }

    if (mood===26) { // EXCLAIM — ! marks
        for (let r=0;r<8;r++) for (let c=0;c<6;c++) shape[r][c]=false;
        // Exclamation mark shape
        for (let r=0;r<4;r++) { shape[r][2]=true; shape[r][3]=true; }
        shape[5][2]=true; shape[5][3]=true; // dot
        // Flash effect
        if (Math.floor(t/300)%2===0) { shape[6][2]=true; shape[6][3]=true; }
    }

    if (mood===27) { // QUESTION — ? marks
        for (let r=0;r<8;r++) for (let c=0;c<6;c++) shape[r][c]=false;
        // ? shape
        shape[0][1]=true;shape[0][2]=true;shape[0][3]=true;
        shape[1][3]=true;shape[1][4]=true;
        shape[2][3]=true;shape[3][2]=true;shape[3][3]=true;
        shape[4][2]=true;
        shape[6][2]=true; // dot
        // Wobble
        fX += Math.round(Math.sin(t/500)*1);
    }

    if (mood===28) { // FEAST — eating cookie
        for (let r=0;r<8;r++) for (let c=0;c<6;c++) shape[r][c]=false;
        // Cookie shape
        const cookie = [[2,1],[2,2],[2,3],[3,0],[3,1],[3,2],[3,3],[3,4],[4,1],[4,2],[4,3]];
        cookie.forEach(([r,c]) => shape[r][c]=true);
        // Chocolate chips
        if (Math.floor(t/300)%2===0) { shape[2][2]=false; shape[3][1]=false; shape[4][3]=false; }
        // Mouth moving
        const mouth = Math.floor(t/200)%2;
        fY += mouth;
    }
    for (let r=0;r<8;r++) for (let c=0;c<6;c++)
        if (shape[r][c]) px(r+fY, startC+c+fX, true);
}

function updateMatrix() {
    const now = Date.now();
    if (now - lastUpdate < 80) { animationFrame = requestAnimationFrame(updateMatrix); return; }
    lastUpdate = now;
    LEDS.forEach(l => l.classList.remove('on'));

    const mood = MOOD_IDS[currentMoodTag] ?? 0;

    if (currentMode === 'SHOUT') {
        let x = shoutPos;
        for (const ch of shoutText) {
            const d = FONT[ch.toUpperCase()] || [31,31,31];
            d.forEach((col,ci) => { for (let r=0;r<5;r++) if ((col>>r)&1) px(r+1, x+ci, true); });
            x += d.length + 1;
        }
        shoutPos--;
        if (shoutPos < -(shoutText.length*5)) shoutPos = 32;
    } else {
        // Blink state machine (matches C++ animRoboEyes)
        blinkCounter++;
        if (blinkPhase === 0) { if (blinkCounter > 40 && Math.random() < 0.08) { blinkPhase = 1; blinkCounter = 0; } }
        else { blinkPhase++; if (blinkPhase > 3) blinkPhase = 0; }

        // Eye drift (matches C++ roboMoveTimer logic)
        if (moveTimer <= 0) {
            if (Math.random() < 0.6) { eyeX = 0; eyeY = 0; moveTimer = 20+Math.floor(Math.random()*30); }
            else { eyeX = Math.floor(Math.random()*5)-2; eyeY = Math.floor(Math.random()*3)-1; moveTimer = 10+Math.floor(Math.random()*20); }
        } else { moveTimer--; }

        drawRoboEye(6, mood, blinkPhase, true);
        drawRoboEye(20, mood, blinkPhase, false);
    }
    animationFrame = requestAnimationFrame(updateMatrix);
}

const FONT = {
    '0':[31,17,31],'1':[0,31,0],'2':[29,21,23],'3':[21,21,31],'4':[7,4,31],
    '5':[23,21,29],'6':[31,21,29],'7':[1,1,31],'8':[31,21,31],'9':[23,21,31],
    ' ':[0,0],'A':[30,5,30],'B':[31,21,10],'C':[31,17,17],'D':[31,17,14],
    'E':[31,21,17],'F':[31,5,1],'G':[31,17,29],'H':[31,4,31],'I':[17,31,17],
    'K':[31,4,27],'L':[31,16,16],'M':[31,2,31],'N':[31,2,31],'O':[31,17,31],
    'P':[31,5,2],'R':[31,5,26],'S':[18,21,9],'T':[1,31,1],'U':[31,16,31],
    '%':[2,5,8],'+':[4,14,4],'-':[4,4,4],'.':[16],'!':[0,23,0],'?':[1,21,2]
};

// Intercepts — instant virtual grid response
const originalControl = control;
control = function(action, value) {
    originalControl(action, value);
    if (action === 'mood') {
        currentMode = 'EYES';
        currentMoodTag = (typeof value === 'string') ? value.toUpperCase() : value;
        updateMood(currentMoodTag);
        eyeX = 0; eyeY = 0; blinkPhase = 0;
    } else if (action === 'shout') {
        currentMode = 'SHOUT';
        shoutText = (typeof value === 'object' ? value.msg : value) || '';
        shoutPos = 32;
        setTimeout(() => { currentMode = 'EYES'; }, 5000);
    } else if (action === 'mode') {
        currentMode = 'EYES';
    }
};

const originalTriggerStat = triggerStat;
triggerStat = function(name) {
    originalTriggerStat(name);
    currentMode = 'SHOUT';
    shoutText = name.toUpperCase();
    shoutPos = 32;
    setTimeout(() => { currentMode = 'EYES'; }, 3000);
};

initMatrix();
updateMatrix();

setInterval(async () => {
    try {
        const r = await fetch('/audio_data');
        const d = await r.json();
        const p = d.peak / 32768;
        if (p > 0.05) audioPeak = Math.max(audioPeak, p);
        else audioPeak *= 0.8;
    } catch(e) {}
}, 50);
