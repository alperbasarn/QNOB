# -*- coding: utf-8 -*-
"""
Created on Fri Mar 28 02:29:29 2025

@author: Alper Basaran
"""

import tkinter as tk
from tkinter import ttk
import time

# Import the separated subtab classes
from serial_subtab import SerialSubTab
from tcp_subtab import TCPSubTab
from mqtt_subtab import MQTTSubTab

class ConnectTab:
    def __init__(self, notebook, app):
        self.app = app
        self.frame = ttk.Frame(notebook)
        
        # Create subtabs
        self.subtabs = ttk.Notebook(self.frame)
        self.subtabs.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Create Serial, TCP, and MQTT subtabs
        self.serial_tab = SerialSubTab(self.subtabs, self.app)
        self.tcp_tab = TCPSubTab(self.subtabs, self.app)
        self.mqtt_tab = MQTTSubTab(self.subtabs, self.app)
        
        self.subtabs.add(self.serial_tab.frame, text="Serial")
        self.subtabs.add(self.tcp_tab.frame, text="TCP")
        self.subtabs.add(self.mqtt_tab.frame, text="MQTT")
        
        # Keep reference to message logs for centralized logging
        self.message_logs = {
            "serial_sent": self.serial_tab.sent_messages,
            "serial_received": self.serial_tab.received_messages,
            "tcp_sent": self.tcp_tab.sent_messages,
            "tcp_received": self.tcp_tab.received_messages,
            "mqtt_sent": self.mqtt_tab.sent_messages,
            "mqtt_received": self.mqtt_tab.received_messages
        }
    
    def log_message(self, message, message_type="status"):
        """Log message to appropriate text area based on type"""
        timestamp = time.strftime("%H:%M:%S")
        formatted_message = f"[{timestamp}] {message}\n"
        
        # Serial-specific messages or generic when serial is connected
        if message_type.startswith("serial") or (message_type in ["sent", "received"] and self.app.connection_type == "serial"):
            # Clean up message type for the subtab
            tab_message_type = message_type
            if message_type == "sent" and self.app.connection_type == "serial":
                tab_message_type = "serial_sent"
            elif message_type == "received" and self.app.connection_type == "serial":
                tab_message_type = "serial_received"
                
            # Direct call to appropriate methods in serial tab
            if tab_message_type in ["serial_sent", "sent"]:
                self.serial_tab.sent_messages.config(state=tk.NORMAL)
                self.serial_tab.sent_messages.insert(tk.END, formatted_message)
                self.serial_tab.sent_messages.see(tk.END)
                self.serial_tab.sent_messages.config(state=tk.DISABLED)
                
            if tab_message_type in ["serial_received", "received"]:
                self.serial_tab.received_messages.config(state=tk.NORMAL)
                self.serial_tab.received_messages.insert(tk.END, formatted_message)
                self.serial_tab.received_messages.see(tk.END)
                self.serial_tab.received_messages.config(state=tk.DISABLED)
        
        # TCP-specific messages or generic when TCP is connected
        if message_type.startswith("tcp") or (message_type in ["sent", "received"] and self.app.connection_type == "tcp"):
            # Clean up message type for the subtab
            tab_message_type = message_type
            if message_type == "sent" and self.app.connection_type == "tcp":
                tab_message_type = "tcp_sent"
            elif message_type == "received" and self.app.connection_type == "tcp":
                tab_message_type = "tcp_received"
                
            # Direct call to appropriate methods in TCP tab
            if tab_message_type in ["tcp_sent", "sent"]:
                self.tcp_tab.sent_messages.config(state=tk.NORMAL)
                self.tcp_tab.sent_messages.insert(tk.END, formatted_message)
                self.tcp_tab.sent_messages.see(tk.END)
                self.tcp_tab.sent_messages.config(state=tk.DISABLED)
                
            if tab_message_type in ["tcp_received", "received"]:
                self.tcp_tab.received_messages.config(state=tk.NORMAL)
                self.tcp_tab.received_messages.insert(tk.END, formatted_message)
                self.tcp_tab.received_messages.see(tk.END)
                self.tcp_tab.received_messages.config(state=tk.DISABLED)
        
        # MQTT-specific messages
        if message_type.startswith("mqtt"):
            # Check if we should filter self messages
            is_self_message = False
            if message_type == "mqtt_received" and self.mqtt_tab.filter_self_messages.get():
                if "sender=" in message:
                    is_self_message = True
            
            # Log sent messages
            if message_type == "mqtt_sent":
                self.mqtt_tab.sent_messages.config(state=tk.NORMAL)
                self.mqtt_tab.sent_messages.insert(tk.END, formatted_message)
                self.mqtt_tab.sent_messages.see(tk.END)
                self.mqtt_tab.sent_messages.config(state=tk.DISABLED)
            
            # Log received messages (if not filtered)
            if message_type == "mqtt_received" and not is_self_message:
                self.mqtt_tab.received_messages.config(state=tk.NORMAL)
                self.mqtt_tab.received_messages.insert(tk.END, formatted_message)
                self.mqtt_tab.received_messages.see(tk.END)
                self.mqtt_tab.received_messages.config(state=tk.DISABLED)
        
        # Status and error messages - show in current tab only
        if message_type in ["status", "error"]:
            # Get current selected tab
            try:
                current_tab = self.subtabs.index(self.subtabs.select())
                
                # Format message with color tag if needed
                tag = message_type if message_type == "error" else "status"
                
                if current_tab == 0:  # Serial tab
                    # Show in both sent and received areas
                    for text_area in [self.serial_tab.sent_messages, self.serial_tab.received_messages]:
                        text_area.config(state=tk.NORMAL)
                        text_area.insert(tk.END, formatted_message, tag)
                        text_area.tag_config("error", foreground="red")
                        text_area.tag_config("status", foreground="purple")
                        text_area.see(tk.END)
                        text_area.config(state=tk.DISABLED)
                
                elif current_tab == 1:  # TCP tab
                    # Show in both sent and received areas
                    for text_area in [self.tcp_tab.sent_messages, self.tcp_tab.received_messages]:
                        text_area.config(state=tk.NORMAL)
                        text_area.insert(tk.END, formatted_message, tag)
                        text_area.tag_config("error", foreground="red")
                        text_area.tag_config("status", foreground="purple")
                        text_area.see(tk.END)
                        text_area.config(state=tk.DISABLED)
                
                elif current_tab == 2:  # MQTT tab
                    # Show in both sent and received areas
                    for text_area in [self.mqtt_tab.sent_messages, self.mqtt_tab.received_messages]:
                        text_area.config(state=tk.NORMAL)
                        text_area.insert(tk.END, formatted_message, tag)
                        text_area.tag_config("error", foreground="red")
                        text_area.tag_config("status", foreground="purple")
                        text_area.see(tk.END)
                        text_area.config(state=tk.DISABLED)
            except:
                # If we can't determine the tab, show nothing
                pass