# -*- coding: utf-8 -*-
"""
Created on Fri Mar 28 03:13:44 2025

@author: Alper Basaran
"""

import tkinter as tk
from tkinter import ttk, scrolledtext
import socket
import threading
import time

class TCPSubTab:
    def __init__(self, notebook, app):
        self.app = app
        self.frame = ttk.Frame(notebook)
        self.tcp_socket = None
        self.reader_thread = None
        self.running = False
        self.server_scan_thread = None
        self.found_servers = []
        
        # Create UI elements
        self.create_ui()
        
        # Initial server scan
        self.scan_for_servers()
        
    def create_ui(self):
        # Top frame for connection controls
        controls_frame = ttk.Frame(self.frame)
        controls_frame.pack(fill=tk.X, padx=5, pady=5)
        
        # Server selection
        ttk.Label(controls_frame, text="Server:").grid(row=0, column=0, padx=5, pady=5, sticky=tk.W)
        self.server_combo = ttk.Combobox(controls_frame, width=30)
        self.server_combo.grid(row=0, column=1, padx=5, pady=5, sticky=tk.W)
        
        # Port
        ttk.Label(controls_frame, text="Port:").grid(row=0, column=2, padx=5, pady=5, sticky=tk.W)
        self.port_entry = ttk.Entry(controls_frame, width=10)
        self.port_entry.grid(row=0, column=3, padx=5, pady=5, sticky=tk.W)
        self.port_entry.insert(0, "23")  # Default telnet port
        
        # Scan, Connect, Disconnect buttons
        self.scan_btn = ttk.Button(controls_frame, text="Scan", command=self.scan_for_servers)
        self.scan_btn.grid(row=0, column=4, padx=5, pady=5)
        
        self.connect_btn = ttk.Button(controls_frame, text="Connect", command=self.connect_tcp)
        self.connect_btn.grid(row=0, column=5, padx=5, pady=5)
        
        self.disconnect_btn = ttk.Button(controls_frame, text="Disconnect", command=self.disconnect_tcp, state=tk.DISABLED)
        self.disconnect_btn.grid(row=0, column=6, padx=5, pady=5)
        
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
        
        # Manual command entry
        command_frame = ttk.Frame(self.frame)
        command_frame.pack(fill=tk.X, padx=5, pady=5)
        
        ttk.Label(command_frame, text="Command:").pack(side=tk.LEFT, padx=5)
        self.command_entry = ttk.Entry(command_frame, width=50)
        self.command_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)
        self.command_entry.bind("<Return>", self.send_command)
        
        self.send_btn = ttk.Button(command_frame, text="Send", command=lambda: self.send_command(None))
        self.send_btn.pack(side=tk.LEFT, padx=5)
        self.send_btn.config(state=tk.DISABLED)
        
        # Scan status
        self.scan_status = ttk.Label(self.frame, text="")
        self.scan_status.pack(fill=tk.X, padx=5, pady=2, anchor=tk.W)
    
    def scan_for_servers(self):
        """Scan the network for TCP servers"""
        if self.server_scan_thread and self.server_scan_thread.is_alive():
            self.app.handle_message("Server scan already in progress", "status")
            return
            
        # Clear previous results
        self.found_servers = []
        self.server_combo['values'] = []
        
        # Update status
        self.scan_status.config(text="Scanning for servers...")
        self.scan_btn.config(state=tk.DISABLED)
        
        # Start scan in background thread
        self.server_scan_thread = threading.Thread(target=self._scan_network, daemon=True)
        self.server_scan_thread.start()
    
    def _scan_network(self):
        """Background thread to scan network for TCP servers"""
        try:
            # Get local IP address
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(("8.8.8.8", 80))
            local_ip = s.getsockname()[0]
            s.close()
            
            # Parse the network prefix
            ip_parts = local_ip.split('.')
            network_prefix = '.'.join(ip_parts[0:3]) + '.'
            
            # Try the IP range starting from .1
            for i in range(1, 255):
                if i % 10 == 0:
                    # Update scan status every 10 hosts
                    self.app.root.after(0, lambda: self.scan_status.config(text=f"Scanning: {network_prefix}{i}/255..."))
                
                target_ip = network_prefix + str(i)
                target_port = int(self.port_entry.get())
                
                try:
                    # Try to connect with a short timeout
                    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    s.settimeout(0.1)
                    result = s.connect_ex((target_ip, target_port))
                    s.close()
                    
                    if result == 0:  # Port is open
                        self.found_servers.append(f"{target_ip}:{target_port}")
                        # Update UI from the main thread
                        self.app.root.after(0, self.update_server_list)
                
                except:
                    # Ignore connection errors
                    pass
            
            # Show final results
            self.app.root.after(0, lambda: self.scan_status.config(text=f"Scan complete: found {len(self.found_servers)} servers"))
            self.app.root.after(0, lambda: self.scan_btn.config(state=tk.NORMAL))
            
            if self.found_servers:
                self.app.handle_message(f"Found {len(self.found_servers)} TCP servers", "status")
            else:
                self.app.handle_message("No TCP servers found", "status")
        
        except Exception as e:
            self.app.root.after(0, lambda: self.scan_status.config(text=f"Scan error: {str(e)}"))
            self.app.root.after(0, lambda: self.scan_btn.config(state=tk.NORMAL))
            self.app.handle_message(f"Server scan error: {str(e)}", "error")
    
    def update_server_list(self):
        """Update the server dropdown with found servers"""
        self.server_combo['values'] = self.found_servers
        if self.found_servers:
            self.server_combo.current(0)
    
    def connect_tcp(self):
        """Connect to the selected TCP server"""
        if not self.server_combo.get() and not self.port_entry.get():
            self.app.handle_message("No server/port specified", "error")
            return
            
        try:
            # Parse server and port
            if ":" in self.server_combo.get():
                server, port = self.server_combo.get().split(":")
                port = int(port)
            else:
                server = self.server_combo.get()
                port = int(self.port_entry.get())
            
            # Create socket and connect
            self.tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.tcp_socket.settimeout(3.0)  # 3 second connection timeout
            self.tcp_socket.connect((server, port))
            self.tcp_socket.settimeout(0.1)  # Short timeout for reads
            
            # Update UI
            self.connect_btn.config(state=tk.DISABLED)
            self.disconnect_btn.config(state=tk.NORMAL)
            self.send_btn.config(state=tk.NORMAL)
            
            # Update app's connection status
            self.app.tcp_connected = True
            self.app.connected_device = self.tcp_socket
            self.app.connection_type = "tcp"
            self.app.update_connection_status()
            
            # Start reader thread
            self.running = True
            self.reader_thread = threading.Thread(target=self.read_tcp, daemon=True)
            self.reader_thread.start()
            
            self.app.handle_message(f"Connected to TCP server at {server}:{port}", "status")
        
        except Exception as e:
            self.app.handle_message(f"TCP connection error: {str(e)}", "error")
    
    def disconnect_tcp(self):
        """Disconnect from the TCP server"""
        if self.tcp_socket:
            self.running = False
            if self.reader_thread:
                self.reader_thread.join(timeout=1.0)
            
            # Close the socket
            try:
                self.tcp_socket.close()
            except:
                pass
            self.tcp_socket = None
            
            # Update UI
            self.connect_btn.config(state=tk.NORMAL)
            self.disconnect_btn.config(state=tk.DISABLED)
            self.send_btn.config(state=tk.DISABLED)
            
            # Update app's connection status
            self.app.tcp_connected = False
            self.app.connected_device = None
            self.app.connection_type = None
            self.app.update_connection_status()
            
            self.app.handle_message("TCP disconnected", "status")
    
    def read_tcp(self):
        """Background thread to read from TCP socket"""
        buffer = ""
        
        while self.running and self.tcp_socket:
            try:
                # Read data if available
                data = self.tcp_socket.recv(1024).decode('utf-8')
                if data:
                    buffer += data
                    
                    # Process complete lines
                    while '\n' in buffer:
                        line, buffer = buffer.split('\n', 1)
                        line = line.strip()
                        if line:
                            # Process the received line
                            self.app.handle_message(line, "tcp_received")
                elif not data:  # Connection closed by server
                    self.app.handle_message("Connection closed by server", "status")
                    self.running = False
                    self.app.root.after(100, self.disconnect_tcp)
                    break
                
                # Short delay to prevent CPU overload
                time.sleep(0.05)
                
            except socket.timeout:
                # Just a timeout, continue
                pass
            except Exception as e:
                self.app.handle_message(f"TCP read error: {str(e)}", "error")
                # On error, break the loop and disconnect
                self.running = False
                self.app.root.after(100, self.disconnect_tcp)
                break
    
    def send_command(self, event=None):
        """Send a command through the TCP socket"""
        if not self.tcp_socket:
            self.app.handle_message("Not connected to TCP server", "error")
            return
            
        command = self.command_entry.get()
        if not command:
            return
            
        # Use the app's central command sender
        if self.app.send_command(command):
            # Clear the entry if sent successfully
            self.command_entry.delete(0, tk.END)
            
    def log_message(self, message, message_type="status"):
        """Log message to the appropriate text area"""
        timestamp = time.strftime("%H:%M:%S")
        formatted_message = f"[{timestamp}] {message}\n"
        
        if message_type in ["serial_sent", "sent"]:
            # Show in sent messages area
            self.sent_messages.config(state=tk.NORMAL)
            self.sent_messages.insert(tk.END, formatted_message)
            self.sent_messages.see(tk.END)
            self.sent_messages.config(state=tk.DISABLED)
            
        elif message_type in ["serial_received", "received"]:
            # Show in received messages area
            self.received_messages.config(state=tk.NORMAL)
            self.received_messages.insert(tk.END, formatted_message)
            self.received_messages.see(tk.END)
            self.received_messages.config(state=tk.DISABLED)
        
        elif message_type == "error":
            # Show errors in both areas with red color
            self.sent_messages.config(state=tk.NORMAL)
            self.sent_messages.insert(tk.END, formatted_message, "error")
            self.sent_messages.tag_config("error", foreground="red")
            self.sent_messages.see(tk.END)
            self.sent_messages.config(state=tk.DISABLED)
            
            self.received_messages.config(state=tk.NORMAL)
            self.received_messages.insert(tk.END, formatted_message, "error")
            self.received_messages.tag_config("error", foreground="red")
            self.received_messages.see(tk.END)
            self.received_messages.config(state=tk.DISABLED)
            
        elif message_type in ["status", "serial_status"]:
            # Show status messages in both areas with purple color
            self.sent_messages.config(state=tk.NORMAL)
            self.sent_messages.insert(tk.END, formatted_message, "status")
            self.sent_messages.tag_config("status", foreground="purple")
            self.sent_messages.see(tk.END)
            self.sent_messages.config(state=tk.DISABLED)
            
            self.received_messages.config(state=tk.NORMAL)
            self.received_messages.insert(tk.END, formatted_message, "status")
            self.received_messages.tag_config("status", foreground="purple")
            self.received_messages.see(tk.END)
            self.received_messages.config(state=tk.DISABLED)