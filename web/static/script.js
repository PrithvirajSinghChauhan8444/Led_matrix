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
    if (tag === 'SCARE' || tag === 'ANGRY') {
        moodBadge.classList.add('glitch');
        moodBadge.style.background = 'linear-gradient(135deg, #ef4444, #dc2626)';
    } else if (tag.startsWith('WEATHER')) {
        moodBadge.classList.remove('glitch');
        moodBadge.style.background = 'linear-gradient(135deg, #eab308, #f97316)';
    } else {
        moodBadge.classList.remove('glitch');
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
    
    try {
        const response = await fetch('/chat', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ messages })
        });
        
        const reader = response.body.getReader();
        const decoder = new TextDecoder("utf-8");
        let assistantReply = "";
        
        while (true) {
            const { value, done } = await reader.read();
            if (done) break;
            const chunk = decoder.decode(value, { stream: true });
            const lines = chunk.split('\n\n');
            
            for (let line of lines) {
                if (line.startsWith('data: ')) {
                    const dataStr = line.substring(6);
                    try {
                        const data = JSON.parse(dataStr);
                        if (data.type === 'tag') {
                            updateMood(data.content);
                        } else if (data.type === 'text') {
                            assistantReply += data.content;
                            bubble.innerHTML = assistantReply + '<span class="blink-cursor"></span>';
                            chatWrapper.scrollTop = chatWrapper.scrollHeight;
                        } else if (data.type === 'done') {
                            setTimeout(() => updateMood("NORMAL"), 3000);
                        }
                    } catch (e) {
                         // ignore incomplete json chunk or empty
                    }
                }
            }
        }
        
        // Finalize UI
        bubble.innerHTML = assistantReply;
        messages.push({ role: "assistant", content: assistantReply });
        
    } catch (error) {
        bubble.innerHTML = "<em>Error connecting to AI.</em>";
    }
}

sendBtn.addEventListener('click', sendMessage);
userInput.addEventListener('keypress', (e) => {
    if (e.key === 'Enter') sendMessage();
});
