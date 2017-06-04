Ocean simulation sample

-Overview:
The C++ AMP ocean simulation sample simulates ocean in real time.

-Hardware requirement:
This sample requires a DirectX 11 capable card. If there is not a DirectX 11 capable card in the system, the sample will use DirectX 11 reference emulator with very low performance.

-Software requirement:
Install June 2010 DirectX SDK from MSDN http://www.microsoft.com/download/en/details.aspx?id=6812
Install Visual Studio 11 from http://msdn.microsoft.com

-Running the sample:
This sample contains the OceanCS project which builds a graphical sample which displays the ocean simulation in a DirectX rendering window.  This sample uses the same DXUT framework and rendering path as the Nvidia SDK sample only modifying the compute portions of the sample to use C++ AMP.

-References:
NVIDIA GPU Computing SDK 4.0\DirectCompute\src\OceanCS

Known issue:
1. Macro redefinition warning/errors.
    This is due to conflict between DirectX SDK version and Visual Studion installed SDK. This shouldnt affect the sample.