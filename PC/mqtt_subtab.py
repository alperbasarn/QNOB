# -*- coding: utf-8 -*-
"""
Created on Fri Mar 28 03:15:00 2025

@author: Alper Basaran
"""

import tkinter as tk
from tkinter import ttk, scrolledtext
import time

class MQTTSubTab:
    def __init__(self, notebook, app):
        self.app = app
        self.frame = ttk.Frame(notebook)
        self.filter_self_messages = tk.BooleanVar(value=True)
        
        # Create UI elements
        self.create_ui()
    
    def create_ui(self):
        # Top frame for connection parameters
        params_frame = ttk.LabelFrame(self.frame, text="MQTT Parameters")
        params_frame.pack(fill=tk.X, padx=5, pady=5)
        
        # Broker URL
        ttk.Label(params_frame, text="Broker URL:").grid(row=0, column=0, padx=5, pady=5, sticky=tk.W)
        self.broker_entry = ttk.Entry(params_frame, width=40)
        self.broker_entry.grid(row=0, column=1, padx=5, pady=5, sticky=tk.W)
        self.broker_entry.insert(0, "broker.hivemq.com")  # Default value
        
        # Port
        ttk.Label(params_frame, text="Port:").grid(row=0, column=2, padx=5, pady=5, sticky=tk.W)
        self.port_entry = ttk.Entry(params_frame, width=6)
        self.port_entry.grid(row=0, column=3, padx=5, pady=5, sticky=tk.W)
        self.port_entry.insert(0, "8883")  # Default value
        
        # Username
        ttk.Label(params_frame, text="Username:").grid(row=1, column=0, padx=5, pady=5, sticky=tk.W)
        self.username_entry = ttk.Entry(params_frame, width=20)
        self.username_entry.grid(row=1, column=1, padx=5, pady=5, sticky=tk.W)
        
        # Password
        ttk.Label(params_frame, text="Password:").grid(row=1, column=2, padx=5, pady=5, sticky=tk.W)
        self.password_entry = ttk.Entry(params_frame, width=20, show="*")
        self.password_entry.grid(row=1, column=3, padx=5, pady=5, sticky=tk.W)
        
        # Control buttons frame
        control_frame = ttk.Frame(self.frame)
        control_frame.pack(fill=tk.X, padx=5, pady=5)
        
        # Edit button
        self.edit_btn = ttk.Button(control_frame, text="Edit", command=self.edit_parameters)
        self.edit_btn.pack(side=tk.LEFT, padx=5, pady=5)
        
        # Connect button
        self.connect_btn = ttk.Button(control_frame, text="Connect MQTT", command=self.connect_mqtt)
        self.connect_btn.pack(side=tk.LEFT, padx=5, pady=5)
        
        # Disconnect button
        self.disconnect_btn = ttk.Button(control_frame, text="Disconnect MQTT", command=self.disconnect_mqtt, state=tk.DISABLED)
        self.disconnect_btn.pack(side=tk.LEFT, padx=5, pady=5)
        
        # Self-message filter checkbox
        self.filter_check = ttk.Checkbutton(control_frame, text="Filter self messages", variable=self.filter_self_messages)
        self.filter_check.pack(side=tk.RIGHT, padx=5, pady=5)
        
        # Status display
        self.status_label = ttk.Label(control_frame, text="Status: Disconnected", foreground="red")
        self.status_label.pack(side=tk.LEFT, padx=20, pady=5)
        
        # Message display area - split into sent and received
        messages_frame = ttk.Frame(self.frame)
        messages_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Sent messages
        ttk.Label(messages_frame, text="Sent Messages:").pack(fill=tk.X, anchor=tk.W)
        self.sent_messages = scrolledtext.ScrolledText(messages_frame, height=10, wrap=tk.WORD)
        self.sent_messages.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        self.sent_messages.config(state=tk.DISABLED)
        
        # Received messages
        ttk.Label(messages_frame, text="Received Messages:").pack(fill=tk.X, anchor=tk.W)
        self.received_messages = scrolledtext.ScrolledText(messages_frame, height=10, wrap=tk.WORD)
        self.received_messages.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        self.received_messages.config(state=tk.DISABLED)
        
        # Topic and message entry
        topic_frame = ttk.Frame(self.frame)
        topic_frame.pack(fill=tk.X, padx=5, pady=5)
        
        # Topic entry
        ttk.Label(topic_frame, text="Topic:").pack(side=tk.LEFT, padx=5)
        self.topic_entry = ttk.Entry(topic_frame, width=30)
        self.topic_entry.pack(side=tk.LEFT, padx=5)
        self.topic_entry.insert(0, "esp32/sound/control")  # Default topic
        
        # Message entry
        message_frame = ttk.Frame(self.frame)
        message_frame.pack(fill=tk.X, padx=5, pady=5)
        
        ttk.Label(message_frame, text="Message:").pack(side=tk.LEFT, padx=5)
        self.message_entry = ttk.Entry(message_frame, width=50)
        self.message_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)
        
        self.send_btn = ttk.Button(message_frame, text="Send", command=self.send_mqtt_message, state=tk.DISABLED)
        self.send_btn.pack(side=tk.LEFT, padx=5)
        
        # Initially disable parameter fields
        self.set_parameters_editable(False)
    
    def set_parameters_editable(self, editable):
        """Enable or disable parameter fields"""
        state = tk.NORMAL if editable else tk.DISABLED
        self.broker_entry.config(state=state)
        self.port_entry.config(state=state)
        self.username_entry.config(state=state)
        self.password_entry.config(state=state)
    
    def edit_parameters(self):
        """Toggle editability of MQTT parameters"""
        current_state = self.broker_entry.cget("state")
        editable = current_state == tk.DISABLED
        self.set_parameters_editable(editable)
        
        # Update button text
        self.edit_btn.config(text="Save" if editable else "Edit")
        
        # If switching to non-editable, save parameters
        if not editable:
            self.save_parameters()
    
    def save_parameters(self):
        """Save MQTT parameters to the app's MQTT handler"""
        if self.app.mqtt_handler:
            broker = self.broker_entry.get()
            port = int(self.port_entry.get())
            username = self.username_entry.get()
            password = self.password_entry.get()
            
            # Update MQTT handler with new parameters
            self.app.mqtt_handler.set_broker(broker, port, username, password)
            
            # Log the action
            self.app.handle_message(f"MQTT parameters updated (Broker: {broker}, Port: {port})", "status")
    
    def connect_mqtt(self):
        """Connect to MQTT broker"""
        # First save parameters in case they were edited
        self.save_parameters()
        
        if self.app.mqtt_handler:
            if self.app.mqtt_handler.connect():
                # Update UI
                self.status_label.config(text="Status: Connected", foreground="green")
                self.connect_btn.config(state=tk.DISABLED)
                self.disconnect_btn.config(state=tk.NORMAL)
                self.send_btn.config(state=tk.NORMAL)
                
                # Log connection
                self.app.handle_message("Connected to MQTT broker", "mqtt_status")
            else:
                self.app.handle_message("Failed to connect to MQTT broker", "error")
    
    def disconnect_mqtt(self):
        """Disconnect from MQTT broker"""
        if self.app.mqtt_handler:
            self.app.mqtt_handler.disconnect()
            
            # Update UI
            self.status_label.config(text="Status: Disconnected", foreground="red")
            self.connect_btn.config(state=tk.NORMAL)
            self.disconnect_btn.config(state=tk.DISABLED)
            self.send_btn.config(state=tk.DISABLED)
            
            # Log disconnection
            self.app.handle_message("Disconnected from MQTT broker", "mqtt_status")
    
    def send_mqtt_message(self):
        """Send MQTT message"""
        if not self.app.mqtt_handler or not self.app.mqtt_handler.is_connected():
            self.app.handle_message("Cannot send message: Not connected to MQTT broker", "error")
            return
        
        topic = self.topic_entry.get()
        message = self.message_entry.get()
        
        if not topic or not message:
            return
        
        # Send message using MQTT handler
        self.app.mqtt_handler.publish(topic, message)
        
        # Clear message entry
        self.message_entry.delete(0, tk.END)
    
    def update_mqtt_status(self, connected, broker=""):
        """Update MQTT connection status display"""
        if connected:
            self.status_label.config(text=f"Status: Connected to {broker}", foreground="green")
            self.connect_btn.config(state=tk.DISABLED)
            self.disconnect_btn.config(state=tk.NORMAL)
            self.send_btn.config(state=tk.NORMAL)
        else:
            self.status_label.config(text="Status: Disconnected", foreground="red")
            self.connect_btn.config(state=tk.NORMAL)
            self.disconnect_btn.config(state=tk.DISABLED)
            self.send_btn.config(state=tk.DISABLED)