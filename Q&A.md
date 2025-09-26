# Sling Programming Language – Q&A  

This file answers common questions and feedback I’ve received about Sling.  

---

## ❓ Why did you create Sling?  
Sling started as a **learning project**. I wanted to understand how compilers, interpreters, and runtimes work at a low level. Instead of only reading about them, I decided to build my own.  

Now, Sling is growing into a **lightweight, embeddable language for microcontrollers** — something easier to read than C, with a small memory footprint.  

---

## ❓ What makes Sling different from other languages?  
- **Focus on embedded systems** – Sling’s runtime is small enough (~50 KB in current builds) to run on older boards like ESP32 or Arduino.  
- **Readable syntax** – Sling is designed to look simpler than C, while still being close enough to “bare metal” for performance.  
- **Header-only embedding** – Sling can be dropped into a C project as a single header file, making it easy to experiment with.  

---

## ❓ Why is `MAX_NATIVE_FUNCS` limited to 256?  
Because Sling targets **low-RAM microcontrollers**, I use a fixed-size array of function pointers. This keeps lookups **fast and predictable** without needing complex memory management. For desktop-scale Sling, this will expand in the future.  

---

## ❓ Isn’t this “reinventing the wheel”?  
Yes and no. Almost every programming language reinvents the wheel in some way. The difference is **why**:  
- For me, Sling is a way to **learn by building**.  
- For embedded devs, Sling could be a **lighter, more readable alternative** to C in small projects.  

---

## ❓ How does Sling compare to Lua?  
Lua is mature, battle-tested, and widely used for embedding. Sling is much younger, smaller, and focused on simplicity. The main difference is:  
- **Lua = general-purpose scripting**  
- **Sling = microcontroller-focused experiments**  

I’m not trying to replace Lua; I’m trying to learn from it while carving a different niche.  

---

## ❓ Are you serious about Sling, or is this a toy?  
Both. Sling is my **serious learning project**. I’ve already built a working interpreter, parser, and runtime in C, and I’m continuing to add features. But I also see value in keeping it **fun and experimental**.  

---

## ❓ Why does the code look messy in places?  
Fair question!  
- Early versions were **header-only** for embedding, which made things cramped.  
- I also experimented with AI assistance in the early stages, which left formatting quirks.  
- I’m actively cleaning this up as I learn better practices.  

---

## ❓ Who is behind Sling?  
I’m Reyaansh Sinha, a **10-year-old developer** who loves compilers, embedded systems, and building things from scratch. Sling is my way of exploring programming deeply and proving what’s possible at any age.  

---

## ❓ What’s next for Sling?  
- Arrays and modules support.  
- Cleaner separation of lexer, parser, and runtime.  
- More examples for microcontrollers.  
- Better documentation.  

---

✨ **Final thought:** Sling is still in progress. It’s not perfect, and it’s not trying to beat C or Python today. But it’s teaching me (and hopefully others) a lot about how languages work under the hood.  
