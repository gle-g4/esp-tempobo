// app-secure.js
// ============================================
// SISTEMA DE VALIDAÇÃO E SEGURANÇA
// ============================================

const SecurityUtils = {
    // Validar número dentro de range
    validateNumber(value, min, max, defaultValue = null) {
        if (typeof value !== 'number' || isNaN(value)) {
            return defaultValue;
        }
        if (value < min || value > max) {
            console.warn(`Valor fora do range: ${value} (esperado: ${min}-${max})`);
            return defaultValue;
        }
        return value;
    },

    // Sanitizar texto para exibição
    sanitizeText(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    },

    // Logger seguro
    log: {
        isProduction: window.location.hostname !== 'localhost',
        
        error(message, error) {
            if (!this.isProduction) {
                console.error(message, error);
            } else {
                console.error('Erro na aplicação');
            }
        },
        
        info(message) {
            if (!this.isProduction) {
                console.log(message);
            }
        }
    }
};

// ============================================
// CONFIGURAÇÃO DO FIREBASE
// ============================================

const database = firebase.database();
const sensorsRef = database.ref('ESP-Projeto-V2/ultimo');
const historicoRef = database.ref('ESP-Projeto-V2/historico');

// Variáveis globais para os gráficos
let temperatureChart, humidityChart, pressureChart, rainChart;
let updateTimeout = null;

// ============================================
// FUNÇÕES AUXILIARES
// ============================================

function formatTime(timestamp) {
    if (!timestamp) return 'Aguardando dados...';
    const date = new Date(timestamp);
    return date.toLocaleTimeString('pt-BR');
}

function formatDate(timestamp) {
    if (!timestamp) return '';
    const date = new Date(timestamp);
    return date.toLocaleDateString('pt-BR');
}

function getTimeAgo(timestamp) {
    if (!timestamp) return 'Aguardando...';
    const now = Date.now();
    const diff = now - timestamp;
    const seconds = Math.floor(diff / 1000);
    const minutes = Math.floor(seconds / 60);
    const hours = Math.floor(minutes / 60);
    
    if (seconds < 60) return 'Agora mesmo';
    if (minutes < 60) return `${minutes} min atrás`;
    if (hours < 24) return `${hours}h atrás`;
    return formatDate(timestamp);
}

function updateConnectionStatus(connected) {
    const statusEl = document.getElementById('connection-status');
    const firebaseStatusEl = document.getElementById('firebase-status');
    
    if (connected) {
        statusEl.innerHTML = '<div class="w-2 h-2 bg-green-500 rounded-full mr-2"></div><span class="text-sm text-green-600">Conectado</span>';
        firebaseStatusEl.textContent = 'Conectado';
        firebaseStatusEl.classList.remove('text-gray-500', 'text-red-600');
        firebaseStatusEl.classList.add('text-green-600');
    } else {
        statusEl.innerHTML = '<div class="w-2 h-2 bg-red-500 rounded-full mr-2 animate-pulse"></div><span class="text-sm text-red-600">Desconectado</span>';
        firebaseStatusEl.textContent = 'Desconectado';
        firebaseStatusEl.classList.remove('text-gray-500', 'text-green-600');
        firebaseStatusEl.classList.add('text-red-600');
    }
}

// ============================================
// ATUALIZAÇÃO SEGURA DE DADOS DO SENSOR
// ============================================

function updateSensorData(data) {
    if (!data) return;
    
    try {
        // Temperatura (DHT) - Validação: -50°C a 100°C
        if (data.DHT && data.DHT.temperatura !== undefined) {
            const temp = SecurityUtils.validateNumber(data.DHT.temperatura, -50, 100);
            if (temp !== null) {
                document.getElementById('temperature').textContent = temp.toFixed(1);
                document.getElementById('temperature').classList.remove('loading-pulse');
                const tempPercent = Math.min(Math.max((temp / 40) * 100, 0), 100);
                document.getElementById('temp-bar').style.width = tempPercent + '%';
            }
        }
        
        // Umidade (DHT) - Validação: 0% a 100%
        if (data.DHT && data.DHT.umidade !== undefined) {
            const humid = SecurityUtils.validateNumber(data.DHT.umidade, 0, 100);
            if (humid !== null) {
                document.getElementById('humidity').textContent = humid.toFixed(0);
                document.getElementById('humidity').classList.remove('loading-pulse');
                document.getElementById('humid-bar').style.width = humid + '%';
            }
        }
        
        // Altitude (BMP280) - Validação: -500m a 9000m
        if (data.BMP280 && data.BMP280.altitude !== undefined) {
            const alt = SecurityUtils.validateNumber(data.BMP280.altitude, -500, 9000);
            if (alt !== null) {
                document.getElementById('altitude').textContent = alt.toFixed(2);
                document.getElementById('altitude').classList.remove('loading-pulse');
                const altPercent = Math.min((alt / 2000) * 100, 100);
                document.getElementById('alt-bar').style.width = altPercent + '%';
            }
        }
        
        // Pressão (BMP280) - Validação: 30000 Pa a 110000 Pa
        if (data.BMP280 && data.BMP280.pressao !== undefined) {
            const press = SecurityUtils.validateNumber(data.BMP280.pressao, 30000, 110000);
            if (press !== null) {
                const pressureHpa = (press / 100).toFixed(2);
                document.getElementById('pressure').textContent = pressureHpa;
                document.getElementById('pressure').classList.remove('loading-pulse');
                const pressPercent = Math.min(Math.max(((pressureHpa - 950) / 100) * 100, 0), 100);
                document.getElementById('press-bar').style.width = pressPercent + '%';
            }
        }
        
        // Chuva - Validação: 0 a 1024
        if (data.chuva !== undefined) {
            let rainValue = data.chuva;
            let rainStatus = "";
            let rainBarWidth = 0;
            
            if (typeof rainValue === 'number') {
                const validRain = SecurityUtils.validateNumber(rainValue, 0, 1024);
                if (validRain !== null) {
                    if (validRain < 300) {
                        rainStatus = "Chuva Intensa";
                        rainBarWidth = 100;
                    } else if (validRain >= 300 && validRain <= 500) {
                        rainStatus = "Chuva Moderada";
                        rainBarWidth = 60;
                    } else {
                        rainStatus = "Sem Chuva";
                        rainBarWidth = 10;
                    }
                    
                    document.getElementById('rain').textContent = validRain;
                    document.getElementById('rain').classList.remove('loading-pulse');
                    
                    const statusContainer = document.getElementById('rain-status-container');
                    if (validRain < 300) {
                        statusContainer.innerHTML = '<span class="text-sm font-semibold text-red-600">' + rainStatus + '</span>';
                    } else if (validRain >= 300 && validRain <= 500) {
                        statusContainer.innerHTML = '<span class="text-sm font-semibold text-yellow-600">' + rainStatus + '</span>';
                    } else {
                        statusContainer.innerHTML = '<span class="text-sm font-semibold text-green-600">' + rainStatus + '</span>';
                    }
                    
                    document.getElementById('rain-bar').style.width = rainBarWidth + '%';
                }
            }
        }
        
        // Timestamp
        const timestamp = data.timestamp || Date.now();
        document.getElementById('last-update-time').textContent = formatTime(timestamp);
        document.getElementById('last-update-date').textContent = formatDate(timestamp);
        document.getElementById('last-update-time').classList.remove('loading-pulse');
        
        const timeAgo = getTimeAgo(timestamp);
        document.querySelectorAll('[id$="-update"]').forEach(el => {
            el.textContent = timeAgo;
        });
        
        document.getElementById('data-collection-status').textContent = 'Ativo - ' + timeAgo;
        
        updateConnectionStatus(true);
        
    } catch (error) {
        SecurityUtils.log.error('Erro ao atualizar dados do sensor:', error);
    }
}

// ============================================
// DEBOUNCE PARA ATUALIZAÇÃO DE GRÁFICOS
// ============================================

function debounce(func, wait) {
    let timeout;
    return function executedFunction(...args) {
        const later = () => {
            clearTimeout(timeout);
            func(...args);
        };
        clearTimeout(timeout);
        timeout = setTimeout(later, wait);
    };
}

// ============================================
// FUNÇÃO PARA ATUALIZAR GRÁFICOS
// ============================================

function updateCharts(historicoData) {
    if (!historicoData) return;

    try {
        const entries = Object.entries(historicoData)
            .map(([key, value]) => ({
                key: key,
                ...value
            }))
            .sort((a, b) => (a.timestamp || 0) - (b.timestamp || 0))
            .slice(-10);

        const labels = [];
        const tempData = [];
        const humidData = [];
        const pressData = [];
        const rainData = [];

        entries.forEach((entry) => {
            let timeLabel = '';
            if (entry.timestamp) {
                const date = new Date(entry.timestamp);
                timeLabel = date.toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit' });
            } else {
                timeLabel = '--:--';
            }
            labels.push(timeLabel);

            // Validar e coletar dados
            const temp = SecurityUtils.validateNumber(entry.DHT?.temperatura, -50, 100, 0);
            const humid = SecurityUtils.validateNumber(entry.DHT?.umidade, 0, 100, 0);
            const press = entry.BMP280?.pressao ? 
                SecurityUtils.validateNumber(entry.BMP280.pressao / 100, 300, 1100, 0) : 0;
            
            let rainValue = 0;
            if (entry.chuva !== undefined) {
                if (typeof entry.chuva === 'number') {
                    rainValue = SecurityUtils.validateNumber(entry.chuva, 0, 1024, 0);
                }
            }

            tempData.push(temp);
            humidData.push(humid);
            pressData.push(press);
            rainData.push(rainValue);
        });

        if (temperatureChart) {
            temperatureChart.data.labels = labels;
            temperatureChart.data.datasets[0].data = tempData;
            temperatureChart.update('none');
        }

        if (humidityChart) {
            humidityChart.data.labels = labels;
            humidityChart.data.datasets[0].data = humidData;
            humidityChart.update('none');
        }

        if (pressureChart) {
            pressureChart.data.labels = labels;
            pressureChart.data.datasets[0].data = pressData;
            pressureChart.update('none');
        }

        if (rainChart) {
            rainChart.data.labels = labels;
            rainChart.data.datasets[0].data = rainData;
            rainChart.update('none');
        }
    } catch (error) {
        SecurityUtils.log.error('Erro ao atualizar gráficos:', error);
    }
}

const updateChartsDebounced = debounce(updateCharts, 1000);

// ============================================
// LISTENERS DO FIREBASE
// ============================================

sensorsRef.on('value', (snapshot) => {
    const data = snapshot.val();
    updateSensorData(data);
}, (error) => {
    SecurityUtils.log.error('Erro ao ler dados do Firebase:', error);
    updateConnectionStatus(false);
});

historicoRef.orderByChild('timestamp').limitToLast(10).on('value', (snapshot) => {
    const historicoData = snapshot.val();
    if (historicoData) {
        updateChartsDebounced(historicoData);
    }
}, (error) => {
    SecurityUtils.log.error('Erro ao ler histórico:', error);
});

// Monitorar conexão
const connectedRef = database.ref('.info/connected');
connectedRef.on('value', (snap) => {
    updateConnectionStatus(snap.val() === true);
});

// ============================================
// INICIALIZAÇÃO DOS GRÁFICOS
// ============================================

document.addEventListener('DOMContentLoaded', function() {
    // Inicializar animações
    AOS.init();
    feather.replace();

    // Vanta.js background
    VANTA.GLOBE({
        el: "#vanta-bg",
        mouseControls: true,
        touchControls: true,
        gyroControls: false,
        minHeight: 200.00,
        minWidth: 200.00,
        scale: 1.00,
        scaleMobile: 1.00,
        color: 0x3a86ff,
        backgroundColor: 0x111827,
        size: 1.00
    });

    // Temperature Chart
    const tempCtx = document.getElementById('temperatureChart').getContext('2d');
    temperatureChart = new Chart(tempCtx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Temperatura (°C)',
                data: [],
                borderColor: '#f87171',
                backgroundColor: 'rgba(248, 113, 113, 0.1)',
                borderWidth: 2,
                tension: 0.3,
                fill: true
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    display: false
                }
            },
            scales: {
                y: {
                    beginAtZero: false
                }
            }
        }
    });

    // Humidity Chart
    const humidityCtx = document.getElementById('humidityChart').getContext('2d');
    humidityChart = new Chart(humidityCtx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Umidade (%)',
                data: [],
                borderColor: '#60a5fa',
                backgroundColor: 'rgba(96, 165, 250, 0.1)',
                borderWidth: 2,
                tension: 0.3,
                fill: true
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    display: false
                }
            },
            scales: {
                y: {
                    beginAtZero: false,
                    min: 0,
                    max: 100
                }
            }
        }
    });

    // Pressure Chart
    const pressureCtx = document.getElementById('pressureChart').getContext('2d');
    pressureChart = new Chart(pressureCtx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Pressão (hPa)',
                data: [],
                borderColor: '#818cf8',
                backgroundColor: 'rgba(129, 140, 248, 0.1)',
                borderWidth: 2,
                tension: 0.3,
                fill: true
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    display: false
                }
            },
            scales: {
                y: {
                    beginAtZero: false
                }
            }
        }
    });

    // Rain Chart
    const rainCtx = document.getElementById('rainChart').getContext('2d');
    rainChart = new Chart(rainCtx, {
        type: 'bar',
        data: {
            labels: [],
            datasets: [{
                label: 'Sensor Chuva (ADC)',
                data: [],
                backgroundColor: 'rgba(34, 211, 238, 0.7)',
                borderColor: 'rgba(34, 211, 238, 1)',
                borderWidth: 1
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    display: false
                }
            },
            scales: {
                y: {
                    beginAtZero: true,
                    max: 1024,
                    reverse: true
                }
            }
        }
    });
});
