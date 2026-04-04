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
                        } else if (data.type === 'text') {
                            assistantReply += data.content;
                            bubble.innerHTML = assistantReply + '<span class="blink-cursor"></span>';
                            chatWrapper.scrollTop = chatWrapper.scrollHeight;
                        } else if (data.type === 'done') {
                            setTimeout(() => updateMood("NORMAL"), 3000);
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
        if (assistantReply) {
             messages.push({ role: "assistant", content: assistantReply });
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
