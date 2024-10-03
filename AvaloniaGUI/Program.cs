using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using Avalonia;
using Avalonia.Controls;
using Avalonia.ReactiveUI;
using Avalonia.VisualTree;
using AvaloniaGUI.Views;

namespace AvaloniaGUI;

class Program
{

	// Initialization code. Don't use any Avalonia, third-party APIs or any
	// SynchronizationContext-reliant code before AppMain is called: things aren't initialized
	// yet and stuff might break.
	[STAThread]
    public static void Main(string[] args)
    {
        Debug.WriteLine(Directory.GetCurrentDirectory());
		BuildAvaloniaApp()
            .StartWithClassicDesktopLifetime(args);
	}

    // Avalonia configuration, don't remove; also used by visual designer.
    public static AppBuilder BuildAvaloniaApp()
        => AppBuilder.Configure<App>()
            .UsePlatformDetect()
            .WithInterFont()
            .LogToTrace()
            .UseReactiveUI();


}
