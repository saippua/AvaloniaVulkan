using Avalonia.Controls;
using System;
using System.Runtime.InteropServices;
using AvaloniaGUI.ViewModels;

namespace AvaloniaGUI.Views
{
	public partial class PerformanceMonitor : UserControl
	{
		public PerformanceMonitor()
		{
			InitializeComponent();
			DataContext = new PerformanceMonitorViewModel();
		}
	}
}
