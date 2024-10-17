#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>

// Definições de pinos
const int DHT_PIN = 15;        // Pino do sensor DHT22
const int LED_PIN = 5;         // Pino do LED (alterado para GPIO5)

// Configurações de rede e MQTT
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "test.mosquitto.org";

// Estrutura para armazenar informações dos integrantes
struct Integrante {
    const char* nome;
    const char* rm;
    float tempLimite;
    bool ledStatus;
};

// Array com informações dos integrantes
Integrante integrantes[] = {
    {"Nicholas Santos", "RM551809", 30.0, false},
    {"Eduardo Violante", "RM550364", 32.0, false},
    {"Vitor Kubica", "RM98903", 34.0, false},
    {"Beatriz Svestka", "RM551534", 36.0, false},
    {"Pedro Pacheco", "RM98043", 38.0, false}
};

// Objetos para DHT e MQTT
DHTesp dht;
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;

// Função para conectar ao WiFi
void setup_wifi() {
    delay(10);
    Serial.println();
    Serial.print("Conectando-se a ");
    Serial.println(ssid);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("");
    Serial.println("WiFi conectado");
    Serial.println("Endereço IP: ");
    Serial.println(WiFi.localIP());
}

// Função callback para mensagens MQTT recebidas
void callback(char* topic, byte* payload, unsigned int length) {
    // Por enquanto não utilizaremos mensagens recebidas
}

// Função para reconectar ao servidor MQTT
void reconnect() {
    while (!client.connected()) {
        Serial.print("Tentando conexão MQTT...");
        String clientId = "ESP32Client-";
        clientId += String(random(0xffff), HEX);
        
        if (client.connect(clientId.c_str())) {
            Serial.println("Conectado");
            client.publish("/grupo4/status", "Sistema Iniciado");
            client.subscribe("/grupo4/comandos");
        } else {
            Serial.print("Falha, rc=");
            Serial.print(client.state());
            Serial.println(" tentando novamente em 5 segundos");
            delay(5000);
        }
    }
}

void setup() {
    pinMode(LED_PIN, OUTPUT);  // Configura o pino do LED como saída
    Serial.begin(115200);
    
    // Inicializa conexões
    setup_wifi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
    
    // Inicializa sensor DHT22
    dht.setup(DHT_PIN, DHTesp::DHT22);
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();
    
    unsigned long now = millis();
    if (now - lastMsg > 2000) {  // Publica a cada 2 segundos
        lastMsg = now;
        
        // Lê dados do sensor
        TempAndHumidity data = dht.getTempAndHumidity();
        
        if (!isnan(data.temperature) && !isnan(data.humidity)) {
            // Publica temperatura e umidade
            String temp = String(data.temperature, 2);
            String hum = String(data.humidity, 1);
            
            client.publish("/grupo4/temperatura", temp.c_str());
            client.publish("/grupo4/umidade", hum.c_str());
            
            // Verifica limites de temperatura para cada integrante
            for (int i = 0; i < 5; i++) {
                bool novoStatus = (data.temperature >= integrantes[i].tempLimite);
                
                // Se o status mudou, publica a mudança
                if (novoStatus != integrantes[i].ledStatus) {
                    integrantes[i].ledStatus = novoStatus;
                    String topic = String("/grupo4/led/") + String(integrantes[i].rm);
                    String payload = novoStatus ? "ON" : "OFF";
                    client.publish(topic.c_str(), payload.c_str());
                }
            }
            
            // Controla o LED físico (usando o primeiro integrante como referência)
            digitalWrite(LED_PIN, integrantes[0].ledStatus ? HIGH : LOW);
            
            // Log no console serial
            Serial.println("----------------------------------------");
            Serial.printf("Temperatura: %.2f°C\n", data.temperature);
            Serial.printf("Umidade: %.1f%%\n", data.humidity);
            for (int i = 0; i < 5; i++) {
                Serial.printf("%s (%s): Limite %.1f°C - LED %s\n", 
                    integrantes[i].nome,
                    integrantes[i].rm,
                    integrantes[i].tempLimite,
                    integrantes[i].ledStatus ? "LIGADO" : "DESLIGADO");
            }
            Serial.println("----------------------------------------");
        }
    }
}