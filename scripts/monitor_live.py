#!/usr/bin/env python3
# *
# * Copyright © 2024-2026 Intel Corporation
# *
# * Permission is hereby granted, free of charge, to any person obtaining a
# * copy of this software and associated documentation files (the "Software"),
# * to deal in the Software without restriction, including without limitation
# * the rights to use, copy, modify, merge, publish, distribute, sublicense,
# * and/or sell copies of the Software, and to permit persons to whom the
# * Software is furnished to do so, subject to the following conditions:
# *
# * The above copyright notice and this permission notice (including the next
# * paragraph) shall be included in all copies or substantial portions of the
# * Software.
# *
# * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# * IN THE SOFTWARE.
#
"""
Live monitoring and visualization of swgenlock synchronization status.
Plots real-time delta values from all secondary nodes collected by primary (-M flag).
"""

import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.ticker import MaxNLocator, FuncFormatter
from matplotlib.widgets import CheckButtons
from matplotlib import cm
import pandas as pd
import numpy as np
import sys
import os
import glob
from collections import defaultdict
import argparse

class SyncStatusPlotter:
	def __init__(self, csv_file, window_seconds=60, skip_initial_seconds=0, filter_outliers=False, y_limit=500, update_interval=1.0, filter_machines=None):
		self.csv_file = csv_file
		self.window_seconds = window_seconds
		self.skip_initial_seconds = skip_initial_seconds
		self.filter_outliers = filter_outliers
		self.y_limit = y_limit  # Delta range in microseconds for displaying markers
		self.update_interval = update_interval  # Update interval in seconds
		self.filter_machines = filter_machines  # List of keywords to filter machines

		# Create figure with space for checkbox and info panel
		self.fig = plt.figure(figsize=(16, 10))
		# Main plot in upper half
		self.ax = plt.subplot2grid((20, 5), (0, 0), rowspan=10, colspan=5)
		# Bottom half: Machine info panel (left) and checkbox (right) - moved down for spacing
		self.info_ax = plt.subplot2grid((20, 5), (12, 0), rowspan=7, colspan=4)
		self.checkbox_ax = plt.subplot2grid((20, 5), (12, 4), rowspan=2, colspan=1)

		# Data storage: {node_name: {'time': [], 'delta': []}}
		self.data = defaultdict(lambda: {'time': [], 'delta': []})
		self.start_time = None
		self.last_file_size = 0
		self.colors = {}
		# Use colormap for unlimited color generation (supports 64+ displays)
		self.colormap = cm.get_cmap('tab20c')  # Can also use 'hsv', 'tab20', 'tab20b', etc.
		self.color_index = 0

		# Show all data flag
		self.show_all_data = False

		# Setup plot, info panel, and checkbox
		self.setup_plot()
		self.setup_info_panel()
		self.setup_checkbox()

	def format_time(self, seconds, pos):
		"""Format time as HH:MM:SS, MM:SS, or SS depending on duration"""
		hours = int(seconds // 3600)
		minutes = int((seconds % 3600) // 60)
		secs = int(seconds % 60)

		if hours > 0:
			return f"{hours}:{minutes:02d}:{secs:02d}"
		elif minutes > 0:
			return f"{minutes}:{secs:02d}"
		else:
			return f"{secs}"

	def setup_plot(self):
		"""Configure the plot appearance"""
		self.ax.set_xlabel('Time (hr:min:sec)', fontsize=13, fontweight='bold', labelpad=10)
		self.ax.set_ylabel('Delta (microseconds)', fontsize=13, fontweight='bold', labelpad=10)
		self.ax.set_title('swgenlock Monitor - Secondary Sync Delta from Primary',
						 fontsize=14, fontweight='bold', pad=8)

		# Add zero reference line (PRIMARY marker)
		self.ax.axhline(y=0, color='red', linestyle='--', linewidth=2.5,
					   alpha=0.8, label='Primary (0 µs)', zorder=10)

		# Grid with dashed lines
		self.ax.grid(True, alpha=0.3, linestyle='--')

		# Format x-axis to show time as HH:MM:SS
		self.ax.xaxis.set_major_formatter(FuncFormatter(self.format_time))

		# Ensure x-axis and y-axis are visible with proper formatting
		self.ax.tick_params(axis='x', which='both', labelsize=10, labelbottom=True)
		self.ax.tick_params(axis='y', which='both', labelsize=10, labelleft=True)

		# Make sure x-axis is at the bottom and visible
		self.ax.xaxis.set_visible(True)
		self.ax.yaxis.set_visible(True)

		# Initial axis limits
		self.ax.set_xlim(0, self.window_seconds)
		self.ax.set_ylim(-self.y_limit, self.y_limit)

	def setup_info_panel(self):
		"""Setup info panel for machine details"""
		self.info_ax.set_xticks([])
		self.info_ax.set_yticks([])
		self.info_ax.spines['top'].set_visible(True)
		self.info_ax.spines['right'].set_visible(True)
		self.info_ax.spines['bottom'].set_visible(True)
		self.info_ax.spines['left'].set_visible(True)
		self.info_ax.set_facecolor('#f9f9f9')
		self.info_ax.set_title('Connected Nodes', fontsize=11, fontweight='bold', pad=10, loc='left')

	def setup_checkbox(self):
		"""Setup checkbox for show all data option"""
		self.checkbox_ax.set_xticks([])
		self.checkbox_ax.set_yticks([])
		self.checkbox_ax.spines['top'].set_visible(False)
		self.checkbox_ax.spines['right'].set_visible(False)
		self.checkbox_ax.spines['bottom'].set_visible(False)
		self.checkbox_ax.spines['left'].set_visible(False)

		# Create checkbox
		self.checkbox = CheckButtons(
			self.checkbox_ax,
			['All Data'],
			[self.show_all_data]
		)
		self.checkbox.on_clicked(self.toggle_show_all)

	def toggle_show_all(self, label):
		"""Toggle between windowed and full data view"""
		self.show_all_data = not self.show_all_data

	def should_include_node(self, node_key):
		"""Check if node matches the filter criteria"""
		if not self.filter_machines:
			return True  # No filter, include all machines

		# Check if node_key (machine:P#) contains any of the filter keywords (case-insensitive)
		for keyword in self.filter_machines:
			if keyword.lower() in node_key.lower():
				return True
		return False

	def update_info_panel(self):
		"""Update the info panel with current node information"""
		self.info_ax.clear()
		self.setup_info_panel()

		if not self.data:
			self.info_ax.text(0.5, 0.5, 'No data yet...',
							ha='center', va='center', fontsize=10, color='gray',
							transform=self.info_ax.transAxes)
			return

		# Prepare node information - start with PRIMARY
		node_info = [('PRIMARY:P0', 0.0, 'red')]

		for node_key in sorted(self.data.keys()):
			node_data = self.data[node_key]
			if node_data['time']:
				latest_delta = node_data['delta'][-1]
				color = self.get_color_for_node(node_key)
				node_info.append((node_key, latest_delta, color))

		if len(node_info) == 1:  # Only PRIMARY, no secondaries yet
			return

		# Calculate columns based on number of nodes
		num_nodes = len(node_info)
		if num_nodes <= 16:
			ncols = 2
		elif num_nodes <= 32:
			ncols = 3
		elif num_nodes <= 48:
			ncols = 4
		else:
			ncols = 5

		# Calculate rows needed
		nodes_per_col = (num_nodes + ncols - 1) // ncols

		# Display nodes in columns
		col_width = 1.0 / ncols
		y_start = 0.90  # Start position for first node
		y_spacing = 0.88 / max(nodes_per_col, 1)  # Tighter spacing

		for idx, (node_name, latest_delta, color) in enumerate(node_info):
			col = idx // nodes_per_col
			row = idx % nodes_per_col

			x_pos = col * col_width + 0.02
			y_pos = y_start - (row * y_spacing)

			# Color box
			self.info_ax.add_patch(plt.Rectangle((x_pos, y_pos - 0.015), 0.015, 0.025,
												transform=self.info_ax.transAxes,
												facecolor=color, edgecolor='black', linewidth=0.5))

			# Node name (includes pipe) and delta
			label_text = f"{node_name}: {latest_delta:+.1f}µs"
			self.info_ax.text(x_pos + 0.022, y_pos, label_text,
							transform=self.info_ax.transAxes,
							fontsize=8, verticalalignment='center')

	def get_color_for_node(self, node_name):
		"""Assign a consistent color to each node using colormap"""
		if node_name not in self.colors:
			# Generate color from colormap (supports unlimited displays)
			# Use golden ratio for better color distribution
			color_value = (self.color_index * 0.618033988749895) % 1.0
			self.colors[node_name] = self.colormap(color_value)
			self.color_index += 1
		return self.colors[node_name]

	def read_new_data(self):
		"""Read new data from CSV file"""
		try:
			# Check if file exists and has new data
			if not os.path.exists(self.csv_file):
				return False

			current_size = os.path.getsize(self.csv_file)
			if current_size == self.last_file_size:
				return False  # No new data

			self.last_file_size = current_size

			# Read CSV file
			df = pd.read_csv(self.csv_file)

			if df.empty:
				return False

			# Set start time from first timestamp if not set
			if self.start_time is None:
				self.start_time = df['timestamp_us'].iloc[0]

			# Process each row
			for _, row in df.iterrows():
				node_name = row['node_name']
				pipe = row['pipe']
				timestamp_us = row['timestamp_us']
				delta_us = row['delta_us']

				# Create unique key combining node name and pipe
				node_key = f"{node_name}:P{pipe}"

				# Apply machine filter (checks full node_key including pipe)
				if not self.should_include_node(node_key):
					continue

				# Convert timestamp to relative seconds from start
				time_sec = (timestamp_us - self.start_time) / 1_000_000.0

				# Store data
				if time_sec not in self.data[node_key]['time']:  # Avoid duplicates
					self.data[node_key]['time'].append(time_sec)
					self.data[node_key]['delta'].append(delta_us)

			return True
		except Exception as e:
			print(f"Error reading CSV: {e}")
			return False

	def update_plot(self, frame):
		"""Animation update function"""
		# Read new data
		self.read_new_data()

		# Clear and redraw
		self.ax.clear()
		self.setup_plot()

		if not self.data:
			return

		# Calculate current time window
		max_time = max([max(node_data['time']) if node_data['time'] else 0
					   for node_data in self.data.values()])

		# Determine window bounds based on checkbox state
		if self.show_all_data:
			# Show all data from beginning
			window_start = 0
			window_end = max(max_time, self.window_seconds)  # At least window_seconds for initial view
		else:
			# Show sliding window
			if max_time > self.window_seconds:
				window_start = max_time - self.window_seconds
				window_end = max_time
			else:
				window_start = 0
				window_end = self.window_seconds

		# Plot data for each node:pipe combination
		for node_key, node_data in sorted(self.data.items()):
			if not node_data['time']:
				continue

			# Filter data within window and skip initial period
			times = []
			deltas = []
			for t, d in zip(node_data['time'], node_data['delta']):
				# Skip initial adjustment period
				if t < self.skip_initial_seconds:
					continue

				if window_start <= t <= window_end:
					times.append(t)
					deltas.append(d)

			if times:
				color = self.get_color_for_node(node_key)

				# Plot line for all points
				self.ax.plot(times, deltas, linestyle='-', linewidth=1.5,
						   label=node_key, color=color, alpha=0.8)

				# Plot markers only for points within y_limit range
				in_range_times = []
				in_range_deltas = []
				for t, d in zip(times, deltas):
					if -self.y_limit <= d <= self.y_limit:
						in_range_times.append(t)
						in_range_deltas.append(d)

				if in_range_times:
					self.ax.plot(in_range_times, in_range_deltas, marker='o',
							   markersize=4, linestyle='', color=color, alpha=0.8)

		# Update axis limits
		self.ax.set_xlim(window_start, window_end)

		# Auto-scale Y axis based on visible data (excluding initial period)
		all_deltas = []
		for node_data in self.data.values():
			for t, d in zip(node_data['time'], node_data['delta']):
				# Skip initial adjustment period
				if t < self.skip_initial_seconds:
					continue
				if window_start <= t <= window_end:
					all_deltas.append(d)

		# Set Y-axis limits
		if all_deltas and self.filter_outliers:
			# Use auto-scaling with outlier filtering if enabled
			if len(all_deltas) > 10:
				deltas_array = np.array(all_deltas)
				q1 = np.percentile(deltas_array, 25)
				q3 = np.percentile(deltas_array, 75)
				iqr = q3 - q1
				lower_bound = q1 - 3 * iqr
				upper_bound = q3 + 3 * iqr
				filtered_deltas = [d for d in all_deltas if lower_bound <= d <= upper_bound]
				if filtered_deltas:
					delta_range = max(abs(min(filtered_deltas)), abs(max(filtered_deltas)))
					margin = delta_range * 0.2
					self.ax.set_ylim(-delta_range - margin, delta_range + margin)
				else:
					self.ax.set_ylim(-self.y_limit, self.y_limit)
			else:
				self.ax.set_ylim(-self.y_limit, self.y_limit)
		else:
			# Use fixed y_limit parameter
			self.ax.set_ylim(-self.y_limit, self.y_limit)

		# Update info panel with node details
		self.update_info_panel()

		# Status text
		if max_time > 0:
			elapsed_str = self.format_time(max_time, None)
			mode_text = "All Data" if self.show_all_data else f"Last {self.window_seconds}s"
			status_text = f"Elapsed: {elapsed_str} | Nodes: {len(self.data)} | Mode: {mode_text}"
			self.ax.text(0.02, 0.98, status_text, transform=self.ax.transAxes,
						fontsize=10, verticalalignment='top',
						bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))

	def run(self):
		"""Start the live plot"""
		print(f"Monitoring CSV file: {self.csv_file}")
		print(f"Window size: {self.window_seconds} seconds")
		print(f"Update interval: {self.update_interval} seconds")
		print(f"Y-axis range: ±{self.y_limit} µs (markers shown only within range)")
		if self.skip_initial_seconds > 0:
			print(f"Skipping initial {self.skip_initial_seconds} seconds (adjustment period)")
		if self.filter_outliers:
			print(f"Outlier filtering: enabled (auto Y-axis scaling)")
		print("Press Ctrl+C to stop...")

		# Adjust layout to ensure proper spacing
		self.fig.subplots_adjust(bottom=0.05, top=0.96, left=0.06, right=0.97, hspace=0.4, wspace=0.3)

		# Set window title
		self.fig.canvas.manager.set_window_title('swgenlock Monitor - Secondary Sync Delta from Primary')

		# Create animation (interval in milliseconds)
		ani = animation.FuncAnimation(self.fig, self.update_plot,
									 interval=int(self.update_interval * 1000),
									 cache_frame_data=False)

		plt.show()


def find_latest_csv():
	"""Find the most recent swgenlock_monitor_*.csv file"""
	csv_files = glob.glob('swgenlock_monitor_*.csv')
	if not csv_files:
		return None
	# Sort by modification time, most recent first
	csv_files.sort(key=os.path.getmtime, reverse=True)
	return csv_files[0]


def main():
	parser = argparse.ArgumentParser(
		description='Live monitoring and visualization of swgenlock sync status from primary',
		formatter_class=argparse.RawDescriptionHelpFormatter,
		epilog="""
Examples:
  # Auto-detect latest CSV file
  %(prog)s

  # Specify CSV file
  %(prog)s swgenlock_monitor_20260224_143337.csv

  # 2-minute window
  %(prog)s -w 120

  # Update every 4 seconds
  %(prog)s --update 4

  # With outlier filtering
  %(prog)s --filter-outliers

  # Custom Y-axis range (±1000 µs)
  %(prog)s --ylimit 1000

  # Filter to show only specific machines or pipes
  %(prog)s --machines "desk,workstation"
  %(prog)s -m "server-01,node"
  %(prog)s -m "P1"              # Show only pipe 1 from all machines
  %(prog)s -m "desk,P0"         # Show machines with "desk" OR pipe 0
		"""
	)
	parser.add_argument('csv_file', nargs='?', default=None,
					   help='CSV file to monitor (default: auto-detect latest)')
	parser.add_argument('-w', '--window', type=int, default=60,
					   help='Time window in seconds (default: 60)')
	parser.add_argument('-u', '--update', type=float, default=1.0,
					   help='Update interval in seconds for reading CSV (default: 1.0)')
	parser.add_argument('-s', '--skip', type=int, default=0,
					   help='Skip initial N seconds to ignore large adjustment values (default: 0)')
	parser.add_argument('-y', '--ylimit', type=int, default=500,
					   help='Y-axis range in microseconds; markers only shown within ±limit (default: 500)')
	parser.add_argument('-f', '--filter-outliers', action='store_true',
					   help='Enable auto Y-axis scaling with statistical outlier filtering')
	parser.add_argument('-m', '--machines', type=str, default=None,
					   help='Filter machines by comma-separated keywords (e.g., "desk,P1,workstation"). Matches machine name and pipe number (format: machine:P#). Default: show all machines')

	args = parser.parse_args()

	# Determine CSV file
	if args.csv_file:
		csv_file = args.csv_file
	else:
		csv_file = find_latest_csv()
		if not csv_file:
			print("Error: No swgenlock_monitor_*.csv files found in current directory")
			print("Please specify a CSV file or run swgenlock with -M flag first")
			sys.exit(1)
		print(f"Auto-detected: {csv_file}")

	# Check if file exists
	if not os.path.exists(csv_file):
		print(f"Error: CSV file not found: {csv_file}")
		sys.exit(1)

	# Parse machine filter if provided
	filter_machines = None
	if args.machines:
		filter_machines = [k.strip() for k in args.machines.split(',') if k.strip()]
		print(f"Filtering nodes containing keywords: {', '.join(filter_machines)}")

	# Create plotter and run
	plotter = SyncStatusPlotter(csv_file,
							   window_seconds=args.window,
							   skip_initial_seconds=args.skip,
							   filter_outliers=args.filter_outliers,
							   y_limit=args.ylimit,
							   update_interval=args.update,
							   filter_machines=filter_machines)

	try:
		plotter.run()
	except KeyboardInterrupt:
		print("\nStopped by user")
		sys.exit(0)


if __name__ == '__main__':
	main()
