using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Avalonia.Media.TextFormatting;
using Avalonia.Platform;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace AvaloniaGUI.Views
{
	public class RenderSurface : NativeControlHost
	{
		public RenderSurface() { }

		[DllImport("VulkanRenderer.dll")]
		static extern void attachRenderer(IntPtr vulkanPtr, IntPtr handle);
		[DllImport("VulkanRenderer.dll")]
		static extern void detachRenderer(IntPtr vulkanPtr, IntPtr handle);

			
		public IntPtr vulkanPtr { get; private set; }
		public IntPtr Instance { get; private set; }
		public IntPtr Handle { get; private set; }

		protected override void OnLoaded(RoutedEventArgs e)
		{
			vulkanPtr = Engine.Get().vulkanPtr;
			attachRenderer(vulkanPtr, Handle);
			base.OnLoaded(e);
		}

		protected override void OnSizeChanged(SizeChangedEventArgs e)
		{
			base.OnSizeChanged(e);
		}

		protected override void OnUnloaded(RoutedEventArgs e)
		{
			detachRenderer(vulkanPtr, Handle);
			base.OnUnloaded(e);
		}

		protected override IPlatformHandle CreateNativeControlCore(IPlatformHandle parent)
		{
			var instance = Process.GetCurrentProcess().Handle;
			var handle = base.CreateNativeControlCore(parent);
			Handle = handle.Handle;
			Instance = instance;
			return handle;
		}

		protected override void DestroyNativeControlCore(IPlatformHandle control)
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				base.DestroyNativeControlCore(control);
			} else
			{
				base.DestroyNativeControlCore(control);
			}

		}

		private void InitializeComponent()
		{
			AvaloniaXamlLoader.Load(this);
		}
	}
}
