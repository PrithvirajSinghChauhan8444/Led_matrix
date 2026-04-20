const chatWrapper = document.getElementById('chat-wrapper');
const userInput = document.getElementById('user-input');
const sendBtn = document.getElementById('send-btn');
const moodBadge = document.getElementById('mood-badge');

let messages = [];

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
