using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace AvaloniaGUI
{
	public sealed class Engine
	{
		[DllImport("VulkanRenderer.dll")]
		static extern IntPtr initEngine(IntPtr callback);
		[DllImport("VulkanRenderer.dll")]
		static extern void destroyEngine(IntPtr vulkanPtr);

		private static void debugCallback([MarshalAs(UnmanagedType.LPStr)] string msg)
		{
			Debug.WriteLine("Vulkan DLL: " + msg);
		}
		private delegate void DebugCallbackDelegate(string value);
		private DebugCallbackDelegate debugCallbackDelegate { get; set; }

		public IntPtr vulkanPtr { get; private set; }

		private static readonly Lazy<Engine> lazy =
			new Lazy<Engine>(() => new Engine());

		public Engine()
		{
			debugCallbackDelegate = new DebugCallbackDelegate(debugCallback);
			vulkanPtr = initEngine(Marshal.GetFunctionPointerForDelegate(debugCallbackDelegate));
		}

		~Engine()
		{
			destroyEngine(vulkanPtr);
		}

		public static Engine Get()
		{
			return lazy.Value;
		}
	}
}
