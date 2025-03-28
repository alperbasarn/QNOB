# -*- coding: utf-8 -*-
"""
Created on Thu Mar 27 05:53:45 2025

@author: Alper Basaran
"""

import tkinter as tk
from sound_controller_app import SoundControllerApp

def main():
    # Initialize the root window
    root = tk.Tk()
    
    # Create the application
    app = SoundControllerApp(root)
    
    # Start the main event loop
    root.mainloop()

if __name__ == "__main__":
    main()