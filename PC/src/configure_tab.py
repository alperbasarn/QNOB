# -*- coding: utf-8 -*-
"""
Created on Fri Mar 28 02:31:59 2025

@author: Alper Basaran
"""

import tkinter as tk
from tkinter import ttk, messagebox
import json
import re

class ConfigureTab:
    def __init__(self, notebook, app):
        self.app = app
        self.frame = ttk.Frame(notebook)
        
        # Dictionary to track EEPROM values
        self.eeprom_values = {}
        self.original_values = {}
        self.eeprom_fields = {}
        self.modified = False
        
        # Create UI elements
        self.create_ui()
    
    def create_ui(self):
        # Top frame for load/save buttons
        button_frame = ttk.Frame(self.frame)
        button_frame.pack(fill=tk.X, padx=5, pady=5)
        
        self.load_btn = ttk.Button(button_frame, text="Load from Device", command=self.load_configuration)
        self.load_btn.pack(side=tk.LEFT, padx=5, pady=5)
        
        self.save_btn = ttk.Button(button_frame, text="Save to Device", command=self.save_configuration, state=tk.DISABLED)
        self.save_btn.pack(side=tk.LEFT, padx=5, pady=5)
        
        # Status label
        self.status_label = ttk.Label(button_frame, text="")
        self.status_label.pack(side=tk.LEFT, padx=20, pady=5, fill=tk.X, expand=True)
        
        # Scrollable frame for configuration fields
        self.canvas = tk.Canvas(self.frame)
        scrollbar = ttk.Scrollbar(self.frame, orient="vertical", command=self.canvas.yview)
        self.scrollable_frame = ttk.Frame(self.canvas)
        
        self.scrollable_frame.bind(
            "<Configure>",
            lambda e: self.canvas.configure(scrollregion=self.canvas.bbox("all"))
        )
        
        self.canvas.create_window((0, 0), window=self.scrollable_frame, anchor="nw")
        self.canvas.configure(yscrollcommand=scrollbar.set)
        
        self.canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")
        
        # Create field containers to group related settings
        self.wifi_frame = self.create_section_frame("WiFi Settings", 0)
        self.mqtt_frame = self.create_section_frame("MQTT Settings", 1)
        self.device_frame = self.create_section_frame("Device Settings", 2)
        self.misc_frame = self.create_section_frame("Miscellaneous Settings", 3)
        
        # Add a stretch frame at the bottom to push everything up
        stretch_frame = ttk.Frame(self.scrollable_frame)
        stretch_frame.grid(row=4, column=0, sticky="nsew", pady=20)
        self.scrollable_frame.grid_rowconfigure(4, weight=1)
    
    def create_section_frame(self, title, row):
        """Create a labeled frame for a section of settings"""
        section = ttk.LabelFrame(self.scrollable_frame, text=title)
        section.grid(row=row, column=0, sticky="ew", padx=10, pady=5, ipadx=5, ipady=5)
        
        # Make sure the section expands to fill the width
        self.scrollable_frame.grid_columnconfigure(0, weight=1)
        
        return section
    
    def create_field(self, parent, label, key, row, value_type="string", options=None):
        """Create a labeled field for a configuration value"""
        ttk.Label(parent, text=label).grid(row=row, column=0, padx=5, pady=2, sticky=tk.W)
        
        # Create different input widgets based on value type
        if value_type == "boolean":
            var = tk.BooleanVar(value=False)
            field = ttk.Checkbutton(parent, variable=var)
            field.var = var  # Store reference to the variable
        elif value_type == "dropdown" and options:
            var = tk.StringVar(value=options[0] if options else "")
            field = ttk.Combobox(parent, values=options, textvariable=var)
            field.var = var  # Store reference to the variable
            field.state(["readonly"])  # Make dropdown readonly
        else:  # Default to string input
            var = tk.StringVar(value="")
            field = ttk.Entry(parent, textvariable=var)
            field.var = var  # Store reference to the variable
        
        field.grid(row=row, column=1, padx=5, pady=2, sticky=tk.EW)
        
        # Make the entry column expandable
        parent.grid_columnconfigure(1, weight=1)
        
        # Bind modification tracking
        if value_type == "boolean":
            var.trace_add("write", lambda *args, key=key: self.value_changed(key))
        else:
            field.bind("<KeyRelease>", lambda event, key=key: self.value_changed(key))
            if value_type == "dropdown":
                field.bind("<<ComboboxSelected>>", lambda event, key=key: self.value_changed(key))
        
        # Store the field reference
        self.eeprom_fields[key] = {"widget": field, "type": value_type}
        
        return field
    
    def load_configuration(self):
        """Request configuration from the device"""
        if not self.app.serial_connected and not self.app.tcp_connected:
            messagebox.showerror("Error", "Not connected to a device")
            return
        
        # Clear any existing fields first
        self.clear_fields()
        
        # Request EEPROM data from device
        self.app.send_command("showNetworks")
        self.app.send_command("commInfo")
        self.app.send_command("showStaticIP")
        
        # Update status
        self.status_label.config(text="Requested configuration from device")
        
        # Dummy data for testing UI - will be replaced with actual data
        # self.populate_test_data()
    
    def clear_fields(self):
        """Clear all configuration fields"""
        # Clear existing fields from the frames
        for widget in self.wifi_frame.winfo_children():
            widget.destroy()
        for widget in self.mqtt_frame.winfo_children():
            widget.destroy()
        for widget in self.device_frame.winfo_children():
            widget.destroy()
        for widget in self.misc_frame.winfo_children():
            widget.destroy()
        
        # Reset tracking dictionaries
        self.eeprom_values = {}
        self.original_values = {}
        self.eeprom_fields = {}
        self.modified = False
        self.save_btn.config(state=tk.DISABLED)
    
    def populate_test_data(self):
        """Populate with test data for UI development"""
        # WiFi settings
        self.create_field(self.wifi_frame, "WiFi SSID (Slot 0):", "wifi0_ssid", 0)
        self.update_field_value("wifi0_ssid", "MyNetwork")
        
        self.create_field(self.wifi_frame, "WiFi Password (Slot 0):", "wifi0_password", 1)
        self.update_field_value("wifi0_password", "password123")
        
        # MQTT settings
        self.create_field(self.mqtt_frame, "Sound MQTT Server:", "sound_mqtt_server", 0)
        self.update_field_value("sound_mqtt_server", "mqtt.example.com")
        
        self.create_field(self.mqtt_frame, "Sound MQTT Port:", "sound_mqtt_port", 1)
        self.update_field_value("sound_mqtt_port", "8883")
        
        # Device settings
        self.create_field(self.device_frame, "Device Name:", "device_name", 0)
        self.update_field_value("device_name", "MyQNOB")
        
        # Set original values to match current values
        self.original_values = self.eeprom_values.copy()
    
    def update_field_value(self, key, value):
        """Update a field's value"""
        if key in self.eeprom_fields:
            field = self.eeprom_fields[key]
            widget = field["widget"]
            field_type = field["type"]
            
            # Update the widget based on its type
            if field_type == "boolean":
                # Convert value to boolean and update
                bool_value = value.lower() in ("true", "yes", "1", "on")
                widget.var.set(bool_value)
            elif field_type == "dropdown":
                # Set dropdown value
                if value in widget["values"]:
                    widget.set(value)
                else:
                    widget.current(0)
            else:
                # Standard string field
                widget.var.set(value)
            
            # Store the value
            self.eeprom_values[key] = value
    
    def process_command_response(self, response):
        """Process a configuration response from the device"""
        # Different response formats require different parsing
        
        # WiFi Networks response
        if "WiFi Networks:" in response:
            match = re.search(r"Slot (\d+): (.+)", response)
            if match:
                slot = match.group(1)
                data = match.group(2)
                
                if "Not Set" in data:
                    # Empty slot
                    ssid_key = f"wifi{slot}_ssid"
                    pass_key = f"wifi{slot}_password"
                    
                    if ssid_key not in self.eeprom_fields:
                        self.create_field(self.wifi_frame, f"WiFi SSID (Slot {slot}):", ssid_key, int(slot)*2)
                    if pass_key not in self.eeprom_fields:
                        self.create_field(self.wifi_frame, f"WiFi Password (Slot {slot}):", pass_key, int(slot)*2+1)
                    
                    self.update_field_value(ssid_key, "")
                    self.update_field_value(pass_key, "")
                else:
                    # Slot with SSID
                    ssid_match = re.search(r"SSID: (.+)", data)
                    if ssid_match:
                        ssid = ssid_match.group(1)
                        ssid_key = f"wifi{slot}_ssid"
                        pass_key = f"wifi{slot}_password"
                        
                        if ssid_key not in self.eeprom_fields:
                            self.create_field(self.wifi_frame, f"WiFi SSID (Slot {slot}):", ssid_key, int(slot)*2)
                        if pass_key not in self.eeprom_fields:
                            self.create_field(self.wifi_frame, f"WiFi Password (Slot {slot}):", pass_key, int(slot)*2+1)
                        
                        self.update_field_value(ssid_key, ssid)
                        # Password is not readable, just set a placeholder
                        self.update_field_value(pass_key, "********")
        
        # Last Used slot
        elif "Last Used:" in response:
            match = re.search(r"Last Used: (\d+)", response)
            if match:
                slot = match.group(1)
                slot_key = "last_wifi_slot"
                
                if slot_key not in self.eeprom_fields:
                    self.create_field(self.wifi_frame, "Last Used WiFi Slot:", slot_key, 6, 
                                     value_type="dropdown", options=["0", "1", "2"])
                
                self.update_field_value(slot_key, slot)
        
        # MQTT Server configurations
        elif "MQTT Server:" in response:
            if "Sound MQTT Server:" in response:
                # Parse Sound MQTT settings
                url_match = re.search(r"URL: (.+)", response)
                port_match = re.search(r"Port: (\d+)", response)
                username_match = re.search(r"Username: (.+)", response)
                
                if url_match:
                    key = "sound_mqtt_url"
                    if key not in self.eeprom_fields:
                        self.create_field(self.mqtt_frame, "Sound MQTT URL:", key, 0)
                    self.update_field_value(key, url_match.group(1).strip())
                
                if port_match:
                    key = "sound_mqtt_port"
                    if key not in self.eeprom_fields:
                        self.create_field(self.mqtt_frame, "Sound MQTT Port:", key, 1)
                    self.update_field_value(key, port_match.group(1).strip())
                
                if username_match:
                    key = "sound_mqtt_username"
                    if key not in self.eeprom_fields:
                        self.create_field(self.mqtt_frame, "Sound MQTT Username:", key, 2)
                    self.update_field_value(key, username_match.group(1).strip())
                    
                    # Add password field (value won't be read from device)
                    key = "sound_mqtt_password"
                    if key not in self.eeprom_fields:
                        self.create_field(self.mqtt_frame, "Sound MQTT Password:", key, 3)
                    self.update_field_value(key, "********")
            
            elif "Light MQTT Server:" in response:
                # Parse Light MQTT settings
                url_match = re.search(r"URL: (.+)", response)
                port_match = re.search(r"Port: (\d+)", response)
                username_match = re.search(r"Username: (.+)", response)
                
                if url_match:
                    key = "light_mqtt_url"
                    if key not in self.eeprom_fields:
                        self.create_field(self.mqtt_frame, "Light MQTT URL:", key, 4)
                    self.update_field_value(key, url_match.group(1).strip())
                
                if port_match:
                    key = "light_mqtt_port"
                    if key not in self.eeprom_fields:
                        self.create_field(self.mqtt_frame, "Light MQTT Port:", key, 5)
                    self.update_field_value(key, port_match.group(1).strip())
                
                if username_match:
                    key = "light_mqtt_username"
                    if key not in self.eeprom_fields:
                        self.create_field(self.mqtt_frame, "Light MQTT Username:", key, 6)
                    self.update_field_value(key, username_match.group(1).strip())
                    
                    # Add password field (value won't be read from device)
                    key = "light_mqtt_password"
                    if key not in self.eeprom_fields:
                        self.create_field(self.mqtt_frame, "Light MQTT Password:", key, 7)
                    self.update_field_value(key, "********")
        
        # Static IP configuration
        elif "Static IP Configuration:" in response:
            enabled_match = re.search(r"Enabled: (.+)", response)
            ip_match = re.search(r"IP: (.+)", response)
            gateway_match = re.search(r"Gateway: (.+)", response)
            subnet_match = re.search(r"Subnet: (.+)", response)
            dns1_match = re.search(r"DNS1: (.+)", response)
            dns2_match = re.search(r"DNS2: (.+)", response)
            
            if enabled_match:
                key = "static_ip_enabled"
                if key not in self.eeprom_fields:
                    self.create_field(self.device_frame, "Static IP Enabled:", key, 0, value_type="boolean")
                self.update_field_value(key, enabled_match.group(1))
            
            if ip_match:
                key = "static_ip"
                if key not in self.eeprom_fields:
                    self.create_field(self.device_frame, "Static IP:", key, 1)
                self.update_field_value(key, ip_match.group(1).strip())
            
            if gateway_match:
                key = "static_gateway"
                if key not in self.eeprom_fields:
                    self.create_field(self.device_frame, "Static Gateway:", key, 2)
                self.update_field_value(key, gateway_match.group(1).strip())
            
            if subnet_match:
                key = "static_subnet"
                if key not in self.eeprom_fields:
                    self.create_field(self.device_frame, "Static Subnet Mask:", key, 3)
                self.update_field_value(key, subnet_match.group(1).strip())
            
            if dns1_match:
                key = "static_dns1"
                if key not in self.eeprom_fields:
                    self.create_field(self.device_frame, "Static DNS 1:", key, 4)
                self.update_field_value(key, dns1_match.group(1).strip())
            
            if dns2_match:
                key = "static_dns2"
                if key not in self.eeprom_fields:
                    self.create_field(self.device_frame, "Static DNS 2:", key, 5)
                self.update_field_value(key, dns2_match.group(1).strip())
        
        # Device name
        elif "Device Name:" in response:
            match = re.search(r"Device Name: (.+)", response)
            if match:
                key = "device_name"
                if key not in self.eeprom_fields:
                    self.create_field(self.device_frame, "Device Name:", key, 6)
                self.update_field_value(key, match.group(1).strip())
        
        # After processing, store original values
        if not self.original_values:
            self.original_values = self.eeprom_values.copy()
            self.status_label.config(text="Configuration loaded successfully")
    
    def value_changed(self, key):
        """Handle when a field value changes"""
        # Get current value
        field = self.eeprom_fields[key]
        widget = field["widget"]
        field_type = field["type"]
        
        # Get the current value based on field type
        if field_type == "boolean":
            current_value = str(widget.var.get()).lower()
        else:
            current_value = widget.var.get()
        
        # Store the new value
        self.eeprom_values[key] = current_value
        
        # Check if changed from original
        if key in self.original_values:
            if current_value != self.original_values[key]:
                # Value changed - highlight in red
                if field_type == "boolean":
                    # For checkboxes, we can't easily change colors
                    pass
                else:
                    widget.config(foreground="red")
                self.modified = True
            else:
                # Value matches original - reset to default color
                if field_type != "boolean":
                    widget.config(foreground="black")
        
        # Enable/disable save button based on modifications
        if self.modified:
            self.save_btn.config(state=tk.NORMAL)
        else:
            self.save_btn.config(state=tk.DISABLED)
        
        # Update status
        self.status_label.config(text="Configuration modified - press Save to apply changes")
    
    def save_configuration(self):
        """Save modified configuration to the device"""
        if not self.app.serial_connected and not self.app.tcp_connected:
            messagebox.showerror("Error", "Not connected to a device")
            return
        
        # Collect modified values
        modified_values = {}
        for key, value in self.eeprom_values.items():
            if key in self.original_values and value != self.original_values[key]:
                modified_values[key] = value
        
        if not modified_values:
            messagebox.showinfo("Info", "No changes to save")
            return
        
        # Process each modified value and send appropriate command
        for key, value in modified_values.items():
            # Device name
            if key == "device_name":
                self.app.send_command(f"setDeviceName:{value}")
            
            # WiFi settings
            elif key.startswith("wifi") and key.endswith("_ssid"):
                slot = key[4:5]  # Extract slot number
                password_key = f"wifi{slot}_password"
                if password_key in modified_values:
                    # Only send if both SSID and password modified
                    self.app.send_command(f"connectWifi:{value}:{modified_values[password_key]}:{slot}")
            
            # Static IP settings
            elif key == "static_ip_enabled":
                if value.lower() in ("true", "yes", "1", "on"):
                    self.app.send_command("enableStaticIP")
                else:
                    self.app.send_command("disableStaticIP")
            elif key == "static_ip":
                # Send full static IP configuration if IP changed
                gateway = self.eeprom_values.get("static_gateway", "192.168.4.1")
                subnet = self.eeprom_values.get("static_subnet", "255.255.255.0")
                dns1 = self.eeprom_values.get("static_dns1", "8.8.8.8")
                dns2 = self.eeprom_values.get("static_dns2", "8.8.4.4")
                self.app.send_command(f"configureStaticIP:{value}:{gateway}:{subnet}:{dns1}:{dns2}")
            
            # Sound MQTT settings
            elif key == "sound_mqtt_url":
                port = self.eeprom_values.get("sound_mqtt_port", "8883")
                username = self.eeprom_values.get("sound_mqtt_username", "")
                password = modified_values.get("sound_mqtt_password", "")
                if password == "********":
                    # Password not changed, use placeholder to indicate no change
                    password = "-"
                self.app.send_command(f"configureSoundMQTTServer:{value}:{port}:{username}:{password}")
            
            # Light MQTT settings 
            elif key == "light_mqtt_url":
                port = self.eeprom_values.get("light_mqtt_port", "8883")
                username = self.eeprom_values.get("light_mqtt_username", "")
                password = modified_values.get("light_mqtt_password", "")
                if password == "********":
                    # Password not changed, use placeholder to indicate no change
                    password = "-"
                self.app.send_command(f"configureLightMQTTServer:{value}:{port}:{username}:{password}")
        
        # Update status
        self.status_label.config(text="Configuration saved to device")
        messagebox.showinfo("Success", "Configuration saved to device")
        
        # Reload configuration to verify changes
        self.load_configuration()