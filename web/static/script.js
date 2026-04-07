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
    moodBadge.classList.remove('glitch');
    if (tag === 'SCARE' || tag === 'ANGRY') {
        moodBadge.classList.add('glitch');
        moodBadge.style.background = 'linear-gradient(135deg, #ef4444, #dc2626)';
    } else if (tag === 'HAPPY' || tag === 'DANCE') {
        moodBadge.style.background = 'linear-gradient(135deg, #22c55e, #16a34a)';
    } else if (tag === 'SAD') {
        moodBadge.style.background = 'linear-gradient(135deg, #3b82f6, #1d4ed8)';
    } else if (tag === 'NAUGHTY' || tag === 'SUSPICIOUS') {
        moodBadge.style.background = 'linear-gradient(135deg, #f97316, #ea580c)';
    } else if (tag === 'JEALOUS') {
        moodBadge.style.background = 'linear-gradient(135deg, #84cc16, #65a30d)';
    } else if (tag === 'SING') {
        moodBadge.style.background = 'linear-gradient(135deg, #06b6d4, #0891b2)';
    } else if (tag === 'STARS') {
        moodBadge.style.background = 'linear-gradient(135deg, #eab308, #ca8a04)';
    } else if (tag === 'SLEEP') {
        moodBadge.style.background = 'linear-gradient(135deg, #6366f1, #4f46e5)';
    } else if (tag === 'BORED') {
        moodBadge.style.background = 'linear-gradient(135deg, #78716c, #57534e)';
    } else if (tag.startsWith('WEATHER')) {
        moodBadge.style.background = 'linear-gradient(135deg, #eab308, #f97316)';
    } else {
        moodBadge.style.background = 'linear-gradient(135deg, #ec4899, #8b5cf6)';
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

function control(action, value) {
    fetch('/control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ action, value })
    });
}

// --- Live mood polling ---
const BAR_COLORS = {
    HAPPY:'#22c55e', SAD:'#3b82f6', ANGRY:'#ef4444', NAUGHTY:'#f97316',
    SUSPICIOUS:'#fb923c', JEALOUS:'#84cc16', ANNOYED:'#f43f5e', SICK:'#a3e635',
    SCARE:'#dc2626', BORED:'#78716c', SLEEP:'#6366f1', STARS:'#eab308',
    DANCE:'#10b981', SING:'#06b6d4', NORMAL:'#8b5cf6'
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
