# 🌡️ ESP-Projeto V2 Dashboard

Dashboard desenvolvido para monitorar em tempo real os dados enviados pelo **ESP8266/ESP32** para o **Firebase Realtime Database**.  
Este projeto faz parte da **EXPOTECH 2025 - UNIFECAF** e integra disciplinas de Programação Orientada a Objetos, Estrutura de dados em Rust, Eletronica Digital e Analogica, Fisica para Sistemas Computacionais, Engenharia de Software e Pesquisa Operacional.

---

## 📡 Visão Geral

O sistema coleta dados ambientais do ESP (como temperatura, umidade, pressão, altitude, entre outros) e exibe essas informações em uma interface web, com atualização contínua.

O dashboard foi projetado para ser simples, leve e fácil de adaptar para diferentes projetos com ESP + Firebase.

---

## 🔥 Funcionalidades

- Atualização em tempo real dos dados do nó `ultimo` no Firebase  
- Exibição organizada dos valores atuais enviados pelo ESP  
- Integração simples com RTDB (Realtime Database)  
- Layout leve e responsivo  
- Compatível com dispositivos móveis  
- Estrutura flexível para evolução (histórico, gráficos, alertas etc.)
- Script gerador de Logs em Rust

---

## 🧰 Tecnologias utilizadas

- **HTML5 / CSS3 / JavaScript**
- **TailwindCSS** (estilização rápida e responsiva)
- **Firebase Realtime Database**
- **Chart.js** (para gráficos quando necessário)
- **AOS** (animations on scroll – opcional, se ativado)
- **ESP8266** como fonte dos dados
- **Rust**
- **Arduino IDE**

---
