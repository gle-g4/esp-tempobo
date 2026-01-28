# 🌡️ TempoBô - Dashboard de Monitoramento ESP

[![EXPOTECH 2025](https://img.shields.io/badge/EXPOTECH-2025-blue)](https://unifecaf.com.br)
[![Firebase](https://img.shields.io/badge/Firebase-RTDB-orange)](https://firebase.google.com/)
[![ESP8266](https://img.shields.io/badge/ESP-8266%2FESP32-red)](https://www.espressif.com/)

> Dashboard de monitoramento em tempo real para dados ambientais coletados por ESP8266/ESP32 e armazenados no Firebase Realtime Database.

## 📋 Sobre o Projeto

Este projeto foi desenvolvido para a **EXPOTECH 2025 - UNIFECAF** e integra conhecimentos de múltiplas disciplinas:

- 💻 Programação Orientada a Objetos
- 🦀 Estrutura de Dados em Rust
- ⚡ Eletrônica Digital e Analógica
- 🔬 Física para Sistemas Computacionais
- 🏗️ Engenharia de Software
- 📊 Pesquisa Operacional

O sistema coleta dados ambientais através do ESP (temperatura, umidade, pressão, altitude, entre outros) e exibe essas informações em uma interface web moderna e responsiva, com atualização contínua e em tempo real.

## ✨ Funcionalidades

- ⚡ **Atualização em Tempo Real**: Dados do nó `ultimo` são sincronizados automaticamente com o Firebase
- 📊 **Visualização Organizada**: Interface clara e intuitiva dos valores enviados pelo ESP
- 🔥 **Integração Firebase**: Conexão simples e direta com o Realtime Database
- 📱 **Design Responsivo**: Layout adaptável para desktop, tablet e smartphones
- 🎨 **Interface Leve**: Carregamento rápido e experiência fluida
- 🔧 **Estrutura Flexível**: Pronta para evolução com histórico, gráficos e alertas
- 📝 **Gerador de Logs**: Script automatizado em Rust para registro de eventos

## 🛠️ Tecnologias Utilizadas

### Frontend
- **HTML5** / **CSS3** / **JavaScript**
- **TailwindCSS** - Framework CSS utilitário para estilização rápida e responsiva
- **Chart.js** - Biblioteca para criação de gráficos (quando necessário)
- **AOS (Animate On Scroll)** - Animações elegantes durante o scroll (opcional)

### Backend & Infraestrutura
- **Firebase Realtime Database** - Banco de dados NoSQL em tempo real
- **ESP8266/ESP32** - Microcontroladores para coleta de dados
- **Rust** - Script para geração de logs
- **Arduino IDE** - Desenvolvimento para ESP

## 🚀 Como Usar

### Pré-requisitos

- Navegador web moderno (Chrome, Firefox, Safari, Edge)
- Conexão com internet
- Dispositivo ESP8266 ou ESP32 configurado
- Projeto Firebase configurado com Realtime Database

### Configuração

1. **Clone o repositório**
```bash
git clone https://github.com/JoaoSouza-ops/esp-projeto-v2-dashboard.git
cd esp-projeto-v2-dashboard
```

2. **Configure o Firebase**
   - Acesse o [Firebase Console](https://console.firebase.google.com/)
   - Crie um novo projeto ou utilize um existente
   - Ative o Realtime Database
   - Copie as credenciais de configuração

3. **Atualize as credenciais no código**
   - Abra o arquivo principal (index.html ou config.js)
   - Insira suas credenciais do Firebase:
   ```javascript
   const firebaseConfig = {
     apiKey: "SUA_API_KEY",
     authDomain: "SEU_AUTH_DOMAIN",
     databaseURL: "SUA_DATABASE_URL",
     projectId: "SEU_PROJECT_ID",
     storageBucket: "SEU_STORAGE_BUCKET",
     messagingSenderId: "SEU_SENDER_ID",
     appId: "SEU_APP_ID"
   };
   ```

4. **Configure o ESP8266/ESP32**
   - Programe seu dispositivo para enviar dados ao Firebase
   - Configure os sensores (DHT, BMP, etc.)
   - Defina o nó `ultimo` como destino dos dados

5. **Abra o Dashboard**
   - Abra o arquivo `index.html` em seu navegador
   - Ou hospede em um servidor web (GitHub Pages, Netlify, Vercel)

## 📂 Estrutura do Projeto

```
esp-projeto-v2-dashboard/
│
├── index.html              # Página principal do dashboard
├── css/
│   └── styles.css         # Estilos customizados
├── js/
│   ├── firebase-config.js # Configuração do Firebase
│   ├── dashboard.js       # Lógica do dashboard
│   └── charts.js          # Configuração dos gráficos
├── rust/
│   └── log-generator/     # Script gerador de logs em Rust
└── assets/
    └── images/            # Imagens e ícones
```

## 📊 Dados Monitorados

O dashboard é capaz de exibir diversos parâmetros ambientais, incluindo:

- 🌡️ **Temperatura** (°C)
- 💧 **Umidade** (%)
- 🎈 **Pressão Atmosférica** (hPa)
- 🏔️ **Altitude** (m)
- E outros sensores customizados

## 🔮 Roadmap

- [ ] Implementar histórico de dados
- [ ] Adicionar gráficos interativos de tendências
- [ ] Sistema de alertas configuráveis
- [ ] Export de dados (CSV/JSON)
- [ ] Dashboard administrativo
- [ ] Suporte multi-dispositivos
- [ ] Modo escuro/claro

## 🤝 Como Contribuir

Contribuições são sempre bem-vindas! Para contribuir:

1. Faça um fork do projeto
2. Crie uma branch para sua feature (`git checkout -b feature/NovaFuncionalidade`)
3. Commit suas mudanças (`git commit -m 'Adiciona nova funcionalidade'`)
4. Push para a branch (`git push origin feature/NovaFuncionalidade`)
5. Abra um Pull Request

## 📝 Licença

Este projeto faz parte da EXPOTECH 2025 - UNIFECAF.

## 👥 Autores

- **João Souza** - [@Joao V. Souza](https://www.linkedin.com/in/joao-v-souza)
- **Gleice Oliveira** - [Gleice Oliveira](https://www.linkedin.com/in/gle-g/)

## 🎓 Instituição

**UNIFECAF - Centro Universitário Católica de Santa Catarina**  
EXPOTECH 2025

## 📞 Contato

Para dúvidas ou sugestões, entre em contato através das issues do GitHub.

---

⭐ Se este projeto foi útil para você, considere dar uma estrela no repositório!
