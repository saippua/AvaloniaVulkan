using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Generators;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Markup.Xaml;
using Avalonia.Platform;
using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using AvaloniaGUI.ViewModels;

namespace AvaloniaGUI.Views;

public partial class MainWindow : Window
{
	public RenderSurface? Surface { get; private set; }

	bool toggle = true;

	void TheButton_OnClick(object? sender, RoutedEventArgs args)
	{
		if (toggle)
		{
			(sender as Button)!.Content = "Detach VkSurface";
			Surface = new RenderSurface();
			VulkanCanvas.Content = Surface;
			WidgetPanel.Children.Add(new PerformanceMonitor());
		} else
		{
			(sender as Button)!.Content = "Reattach VkSurface";
			WidgetPanel.Children.Clear();
			Surface = null;
			VulkanCanvas.Content = null;
		}
		toggle ^= true;
	}
	public MainWindow()
	{
		InitializeComponent();

		TheButton.Click += TheButton_OnClick;
	}
}
