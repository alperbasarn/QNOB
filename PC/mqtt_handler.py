# -*- coding: utf-8 -*-
"""
Created on Thu Mar 27 05:55:51 2025

@author: Alper Basaran
"""

import paho.mqtt.client as mqtt
import ssl
import time

class MQTTHandler:
    """Handles MQTT connection and message processing"""
    
    def __init__(self, message_callback=None):
        # Use the recommended client creation approach
        self.client = mqtt.Client()
        
        # Set callbacks
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_disconnect = self.on_disconnect
        self.client.on_publish = self.on_publish
        
        self.connected = False
        self.message_callback = message_callback
        
        # Default MQTT broker settings
        self.broker_address = "4b664591e7764720a1ae5483e793dfa0.s1.eu.hivemq.cloud"
        self.broker_port = 8883
        self.username = "test123"
        self.password = "Test1234"
        
        # Track last sent setpoint to avoid feedback loops
        self.last_sent_setpoint = None
        
        # Client ID to identify messages from this client
        self.client_id = "PC"
        
        # Reference to the main app (will be set later)
        self._app = None
    
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
            
            self.client.connect(self.broker_address, self.broker_port, 60)
            self.client.loop_start()
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
            if self._app and hasattr(self._app, 'update_media_state'):
                self._app.update_media_state()
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
    
    def on_disconnect(self, client, userdata, rc):
        """Callback when disconnected from MQTT broker"""
        self.connected = False
        reason = "Unexpected disconnection" if rc != 0 else "Requested disconnection"
        if self.message_callback:
            self.message_callback(f"Disconnected from MQTT broker: {reason}", "status")
    
    def on_message(self, client, userdata, msg):
        """Callback when message is received"""
        topic = msg.topic
        try:
            payload = msg.payload.decode()
        except UnicodeDecodeError:
            payload = f"Binary data ({len(msg.payload)} bytes)"
        
        if self.message_callback:
            self.message_callback(f"Received [{topic}]: {payload}", "received")
        
        # Handle state request message
        if topic == "esp32/sound/get_state" and "request" in payload:
            # ESP32 is requesting the current state (during initialization)
            if self.message_callback:
                self.message_callback("Received state request from ESP32", "status")
            
            # Get the current Windows volume and media state
            if self._app and hasattr(self._app, 'audio_controller') and hasattr(self._app, 'media_controller'):
                current_volume = self._app.audio_controller.get_volume_percent()
                # Determine if media is playing using media controller
                is_playing = self._app.media_controller.get_playing_state()
                
                # Update our local state tracking
                self._app.is_playing = is_playing
                self._app.update_playing_ui()
                
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
                if self._app and hasattr(self._app, 'media_controller'):
                    is_playing = self._app.media_controller.play_pause()
                    # If play_pause returned false, it means it couldn't play
                    # or media is now paused
                    self._app.is_playing = is_playing
                    self._app.update_playing_ui()
                    
                    # Send status back to ESP32 to confirm the state
                    state = "playing" if is_playing else "paused"
                    self.publish("esp32/sound/response", state)
                    
                    self.message_callback(f"Media control: play - new state: {state}", "status")
            elif command == "pause":
                # Actually control media playback
                if self._app and hasattr(self._app, 'media_controller'):
                    # If already playing, toggle to pause
                    if self._app.is_playing:
                        is_playing = self._app.media_controller.play_pause()
                    else:
                        is_playing = False
                        
                    self._app.is_playing = is_playing
                    self._app.update_playing_ui()
                    
                    # Send status back to ESP32 to confirm the state
                    state = "playing" if is_playing else "paused"
                    self.publish("esp32/sound/response", state)
                    
                    self.message_callback(f"Media control: pause - new state: {state}", "status")
            elif command == "forward":
                if self._app and hasattr(self._app, 'media_controller'):
                    self._app.media_controller.next_track()
                    self.message_callback("Media control: forward", "status")
            elif command == "rewind":
                if self._app and hasattr(self._app, 'media_controller'):
                    self._app.media_controller.prev_track()
                    self.message_callback("Media control: rewind", "status")
    
    def on_publish(self, client, userdata, mid):
        """Callback when message is published successfully"""
        # This confirms the message was delivered to the broker
        if self.message_callback:
            self.message_callback(f"Message {mid} successfully published", "status")
    
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
                    self.message_callback(f"Sent [{topic}]: {full_message}", "sent")
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
    
    def set_app(self, app):
        """Link this handler with the main app for access to audio controller"""
        self._app = app