using ReactiveUI;
using System;
using System.Diagnostics;
using System.Runtime.InteropServices;


namespace AvaloniaGUI.ViewModels
{
	[StructLayout(LayoutKind.Sequential)]
	public unsafe struct sPerf
	{
		public fixed double frameDelta[100];
		public int currentFrame;
	}

	

	public unsafe class PerformanceMonitorViewModel : ViewModelBase
	{
		[DllImport("VulkanRenderer.dll", CallingConvention = CallingConvention.StdCall)]
		static extern bool setPerformanceMonitorCallback(IntPtr vulkanPtr, IntPtr callback);

		private Stopwatch timer = new();

		private const double FPS_limit = 5;
		private long previous_draw = Stopwatch.GetTimestamp();

		public void nativeCallback(sPerf data)
		{
			
			if (timer.ElapsedMilliseconds > (1000 / FPS_limit))
			{
				double fr = 0;
				for (int i = 0; i < 100; i++)
				{
					fr += data.frameDelta[i];
				}
				Framerate = fr / 100;
				timer.Restart();
			}
		}
		public delegate void NativeCallbackDelegate(sPerf value);
		private NativeCallbackDelegate cb { get; set; }

		public IntPtr vulkanPtr { get; private set; }

		private double framerate = 1;
		public double Framerate
		{
			get => framerate;
			set => this.RaiseAndSetIfChanged(ref framerate, 1.0 / value);
		}

		public void attachVulkan(IntPtr vulkan)
		{
			vulkanPtr = vulkan;
			IntPtr fp = Marshal.GetFunctionPointerForDelegate(cb);
			if (!setPerformanceMonitorCallback(vulkan, fp))
			{
				Debug.WriteLine("Failed to set Performance Monitor callback!");
				throw new ExternalException();
			}
		}

		public PerformanceMonitorViewModel()
		{
			cb = new NativeCallbackDelegate(nativeCallback);
			vulkanPtr = Engine.Get().vulkanPtr;
			timer.Start();
			attachVulkan(vulkanPtr);
			
		}
	}
}
