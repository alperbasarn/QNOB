import paho.mqtt.client as mqtt
import ssl
import time
import threading

class MQTTHandler:
    """Handles MQTT connection and message processing"""
    
    def __init__(self, app, message_callback=None):
        # Use the recommended client creation approach
        self.client = mqtt.Client()
        
        # Set callbacks
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_disconnect = self.on_disconnect
        self.client.on_publish = self.on_publish
        
        self.connected = False
        self.message_callback = message_callback
        self.app = app
        
        # Default MQTT broker settings
        self.broker_address = "4b664591e7764720a1ae5483e793dfa0.s1.eu.hivemq.cloud"
        self.broker_port = 8883
        self.username = "test123"
        self.password = "Test1234"
        
        # Track last sent setpoint to avoid feedback loops
        self.last_sent_setpoint = None
        
        # Client ID to identify messages from this client
        self.client_id = "PC"
    
    def set_broker(self, address, port=8883, username="", password=""):
        """Set MQTT broker parameters"""
        self.broker_address = address
        self.broker_port = port
        self.username = username
        self.password = password
    
    def connect(self):
        """Connect to MQTT broker"""
        try:
            if self.username and self.password:
                self.client.username_pw_set(self.username, self.password)
            
            # Enable TLS/SSL for secure connection
            self.client.tls_set(cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLS)
            self.client.tls_insecure_set(False)
            
            # Connect with timeout
            self.client.connect(self.broker_address, self.broker_port, 60)
            self.client.loop_start()
            
            # Wait for connection to establish or timeout
            start_time = time.time()
            while not self.connected and (time.time() - start_time) < 5:
                time.sleep(0.1)
            
            if not self.connected:
                if self.message_callback:
                    self.message_callback("Connection timeout, could not connect to broker", "error")
                return False
                
            return True
        except Exception as e:
            if self.message_callback:
                self.message_callback(f"Connection error: {str(e)}", "error")
            return False
    
    def disconnect(self):
        """Disconnect from MQTT broker"""
        try:
            self.client.loop_stop()
            self.client.disconnect()
        except Exception as e:
            if self.message_callback:
                self.message_callback(f"Disconnect error: {str(e)}", "error")
    
    def on_connect(self, client, userdata, flags, rc):
        """Callback when connected to MQTT broker"""
        if rc == 0:
            self.connected = True
            if self.message_callback:
                self.message_callback("Connected to MQTT broker", "status")
            
            # Subscribe to ESP32 topics
            self.client.subscribe("esp32/sound/control")
            self.client.subscribe("esp32/sound/setpoint")
            self.client.subscribe("esp32/sound/response")
            self.client.subscribe("esp32/sound/get_state")
            
            # If we have an app reference, update media state to sync with ESP32
            if self.app and hasattr(self.app, 'sound_tab') and hasattr(self.app.sound_tab, 'update_mqtt_status'):
                self.app.sound_tab.update_mqtt_status(True, self.broker_address)
                
        else:
            self.connected = False
            error_messages = {
                1: "Connection refused - incorrect protocol version",
                2: "Connection refused - invalid client identifier",
                3: "Connection refused - server unavailable",
                4: "Connection refused - bad username or password",
                5: "Connection refused - not authorized"
            }
            error_msg = error_messages.get(rc, f"Unknown error code: {rc}")
            if self.message_callback:
                self.message_callback(f"Failed to connect: {error_msg}", "error")
                
            # Update UI if available
            if self.app and hasattr(self.app, 'sound_tab') and hasattr(self.app.sound_tab, 'update_mqtt_status'):
                self.app.sound_tab.update_mqtt_status(False)
    
    def on_disconnect(self, client, userdata, rc):
        """Callback when disconnected from MQTT broker"""
        self.connected = False
        reason = "Unexpected disconnection" if rc != 0 else "Requested disconnection"
        if self.message_callback:
            self.message_callback(f"Disconnected from MQTT broker: {reason}", "status")
            
        # Update UI if available
        if self.app and hasattr(self.app, 'sound_tab') and hasattr(self.app.sound_tab, 'update_mqtt_status'):
            self.app.sound_tab.update_mqtt_status(False)
    
    def on_message(self, client, userdata, msg):
        """Callback when message is received"""
        topic = msg.topic
        try:
            payload = msg.payload.decode()
        except UnicodeDecodeError:
            payload = f"Binary data ({len(msg.payload)} bytes)"
        
        if self.message_callback:
            # Only send with mqtt_received type to prevent duplication
            self.message_callback(f"Received [{topic}]: {payload}", "mqtt_received")
        
        # Handle state request message
        if topic == "esp32/sound/get_state" and "request" in payload:
            # ESP32 is requesting the current state (during initialization)
            if self.message_callback:
                self.message_callback("Received state request from ESP32", "status")
            
            # Get the current Windows volume and media state
            if self.app and hasattr(self.app, 'audio_controller') and hasattr(self.app, 'media_controller'):
                current_volume = self.app.audio_controller.get_volume_percent()
                # Determine if media is playing using media controller
                is_playing = self.app.media_controller.get_playing_state()
                
                # Update our sound tab UI
                if hasattr(self.app, 'sound_tab'):
                    self.app.sound_tab.update_volume(current_volume, send_to_device=False)
                    self.app.sound_tab.update_playing_state(is_playing)
                
                # Create state response message
                state = "playing" if is_playing else "paused"
                response = f"state:{state},volume:{current_volume}"
                
                # Send response
                self.publish("esp32/sound/response", response)
                if self.message_callback:
                    self.message_callback(f"Sent state response: {response}", "sent")
            else:
                if self.message_callback:
                    self.message_callback("Could not get media state, sending default response", "warning")
                    # Send default response
                    self.publish("esp32/sound/response", "state:paused,volume:50")
        
        # Process commands from the ESP32
        elif topic == "esp32/sound/control":
            # Check if the message is from ourselves to avoid feedback loops
            if f"sender={self.client_id}" in payload:
                return
                
            # Extract the actual command (remove sender part if present)
            command = payload
            if "," in payload:
                parts = payload.split(",")
                for part in parts:
                    if not part.startswith("sender="):
                        command = part.strip()
                        break
            
            # Handle media control commands
            if command == "play":
                # Actually control media playback
                if self.app and hasattr(self.app, 'media_controller'):
                    is_playing = self.app.media_controller.play_pause()
                    
                    # Update UI
                    if hasattr(self.app, 'sound_tab'):
                        self.app.sound_tab.update_playing_state(is_playing)
                    
                    # Send status back to ESP32 to confirm the state
                    state = "playing" if is_playing else "paused"
                    self.publish("esp32/sound/response", state)
                    
                    if self.message_callback:
                        self.message_callback(f"Media control: play - new state: {state}", "status")
            elif command == "pause":
                # Actually control media playback
                if self.app and hasattr(self.app, 'media_controller'):
                    # If already playing, toggle to pause
                    if self.app.media_controller.get_playing_state():
                        is_playing = self.app.media_controller.play_pause()
                    else:
                        is_playing = False
                    
                    # Update UI
                    if hasattr(self.app, 'sound_tab'):
                        self.app.sound_tab.update_playing_state(is_playing)
                    
                    # Send status back to ESP32 to confirm the state
                    state = "playing" if is_playing else "paused"
                    self.publish("esp32/sound/response", state)
                    
                    if self.message_callback:
                        self.message_callback(f"Media control: pause - new state: {state}", "status")
            elif command == "forward":
                if self.app and hasattr(self.app, 'media_controller'):
                    self.app.media_controller.next_track()
                    if self.message_callback:
                        self.message_callback("Media control: forward", "status")
            elif command == "rewind":
                if self.app and hasattr(self.app, 'media_controller'):
                    self.app.media_controller.prev_track()
                    if self.message_callback:
                        self.message_callback("Media control: rewind", "status")
        
        # Process setpoint messages
        elif topic == "esp32/sound/setpoint" and "setpoint:" in payload:
            # Check if the message is from ourselves to avoid feedback loops
            if f"sender={self.client_id}" in payload:
                return
                
            # Extract the setpoint value
            try:
                setpoint_str = payload.split("setpoint:")[1].split(",")[0].strip()
                setpoint = int(setpoint_str)
                
                # Update the audio volume in Windows
                if self.app and hasattr(self.app, 'audio_controller'):
                    self.app.audio_controller.set_volume_percent(setpoint)
                    
                if self.message_callback:
                    self.message_callback(f"Received setpoint {setpoint} from ESP32", "status")
            except Exception as e:
                if self.message_callback:
                    self.message_callback(f"Error processing setpoint: {str(e)}", "error")
    
    def on_publish(self, client, userdata, mid):
        """Callback when message is published successfully"""
        # Disabled to avoid extra "successfully published" messages
        pass
    
    def publish(self, topic, message):
        """Publish message to topic with sender ID"""
        if self.connected:
            # Add sender ID to message if not already included
            if not message.startswith(f"sender={self.client_id}"):
                full_message = f"sender={self.client_id},{message}"
            else:
                full_message = message
                
            result = self.client.publish(topic, full_message, qos=1)
            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                if self.message_callback:
                    # Only log the original message the user sent
                    self.message_callback(f"Sent [{topic}]: {message}", "mqtt_sent")
                return True
            else:
                if self.message_callback:
                    self.message_callback(f"Failed to publish message: {mqtt.error_string(result.rc)}", "error")
                return False
        else:
            if self.message_callback:
                self.message_callback("Not connected to MQTT broker", "error")
            return False
    
    def is_connected(self):
        """Check if connected to MQTT broker"""
        return self.connected
        
    def get_broker_info(self):
        """Get the broker address and port"""
        if self.broker_address:
            return f"{self.broker_address}:{self.broker_port}"
        return ""