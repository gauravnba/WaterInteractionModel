//--------------------------------------------------------------------------------------
// File: utilities.h
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

/*
* Copyright 1993-2010 NVIDIA Corporation.  All rights reserved.
*
* Please refer to the NVIDIA end user license agreement (EULA) associated
* with this source code for terms and conditions that govern your use of
* this software. Any use, reproduction, disclosure, or distribution of
* this software and related documentation outside the terms of the EULA
* is strictly prohibited.
*
*/

#ifndef H_UTILITIES_H
#define H_UTILITIES_H

#include <windows.h>
#include <TCHAR.H>
#include <string>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>
#include "DXUT.h"

typedef std::basic_string<TCHAR> tstring; 

static void FMsg( const TCHAR * szFormat, ... )
{	
    static TCHAR buffer[2048];
    va_list args;
    va_start( args, szFormat );
    _vsnwprintf_s( buffer, 2048, szFormat, args );
    va_end( args );
    buffer[2048-1] = '\0';			// terminate in case of overflow
    OutputDebugString( buffer );
}

class NVFileSearch
{
public:
    NVFileSearch::NVFileSearch()
    {
        ZeroMemory(&m_FindData, sizeof(WIN32_FIND_DATA));
    }

    NVFileSearch::~NVFileSearch()
    {

    }

    virtual bool FileFoundCallback(const WIN32_FIND_DATA& FindData, const tstring strDirectory) = 0;

    virtual void FindFile(const tstring strFileString, const tstring strDirectoryStart, bool bRecurse)
    {
        tstring strDirectory;
        strDirectory.resize(MAX_PATH);
        DWORD dwNewSize = GetCurrentDirectory(MAX_PATH, &strDirectory[0]);
        strDirectory.resize(dwNewSize);
        GetCurrentDirectory(dwNewSize, &strDirectory[0]);

        SetCurrentDirectory(strDirectoryStart.c_str());

        WalkDirectory(strFileString, bRecurse);

        SetCurrentDirectory(strDirectory.c_str());
    }

protected:
    virtual void WalkDirectory(const tstring strFileString, bool bRecurse)
    {
        HANDLE hFind;

        m_bRecurse = bRecurse;

        tstring strDirectory;
        strDirectory.resize(MAX_PATH);
        DWORD dwNewSize = GetCurrentDirectory(MAX_PATH, &strDirectory[0]);
        strDirectory.resize(dwNewSize);
        GetCurrentDirectory(dwNewSize, &strDirectory[0]);

        hFind = FindFirstFile(strFileString.c_str(), &m_FindData);

        if (hFind == INVALID_HANDLE_VALUE)
            m_bOK = false;
        else
            m_bOK = true;

        while (m_bOK)
        {
            if (!(m_FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                // Grab the current directory, so that we can reset it after the callback.
                // Otherwise, if the callback changes the current directory in any way,
                // our search will abort prematurely.
                // Alternately, we could require the callback to not have any current
                // directory changes, but this is an easy fix, and performance is not
                // likely an issue.
                tstring strCurrentDirectory;
                strCurrentDirectory.resize(MAX_PATH);
                dwNewSize = GetCurrentDirectory(MAX_PATH, &strCurrentDirectory[0]);
                strCurrentDirectory.resize(dwNewSize);
                GetCurrentDirectory(dwNewSize, &strCurrentDirectory[0]);

                FileFoundCallback(m_FindData, strDirectory);

                // Reset the current directory to what it was before the callback.
                SetCurrentDirectory(strCurrentDirectory.c_str());
            }

            m_bOK = FindNextFile(hFind, &m_FindData);
        }

        if (hFind != INVALID_HANDLE_VALUE)
            FindClose(hFind);

        if (m_bRecurse)
        {
            hFind = FindFirstChildDir();

            if (hFind == INVALID_HANDLE_VALUE)
                m_bOK = false;
            else 
                m_bOK = true;

            while (m_bOK)
            {
                if (SetCurrentDirectory(m_FindData.cFileName))
                {
                    WalkDirectory(strFileString, true);

                    SetCurrentDirectory((TCHAR*)(".."));				
                }
                m_bOK = FindNextChildDir(hFind);
            }

            if (hFind != INVALID_HANDLE_VALUE)
                FindClose(hFind);
        }
    }

    virtual BOOL IsChildDir()
    {
        return ((m_FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (m_FindData.cFileName[0] != '.'));
    }

    virtual BOOL FindNextChildDir(HANDLE hFindFile)
    {
        BOOL bFound = FALSE;
        do
        {
            bFound = FindNextFile(hFindFile, &m_FindData);
        } while (bFound && !IsChildDir());

        return bFound;
    }

    virtual HANDLE FindFirstChildDir()
    {
        BOOL bFound;
        HANDLE hFindFile = FindFirstFile((TCHAR*)("*.*"), &m_FindData);

        if (hFindFile != INVALID_HANDLE_VALUE)
        {
            bFound = IsChildDir();

            if (!bFound)
            {
                bFound = FindNextChildDir(hFindFile);
            }

            if (!bFound)
            {
                FindClose(hFindFile);
                hFindFile = INVALID_HANDLE_VALUE;
            }
        }

        return hFindFile;
    }

protected:
    BOOL m_bRecurse;
    BOOL m_bOK;
    BOOL m_bIsDir;
    WIN32_FIND_DATA m_FindData;
};

namespace GetFilePath
{
    static tstring strStartPath;

    typedef tstring (*GetFilePathFunction)(const tstring &, bool bVerbose );

    // This variable allows us to recursively search for media
    class NVMediaSearch : public NVFileSearch
    {
    public:
        tstring m_strPath;
        virtual bool FileFoundCallback(const WIN32_FIND_DATA& FindData, const tstring& strDir)
        {
            UNREFERENCED_PARAMETER(FindData);
            m_strPath = strDir;

            return false;
        }
    };


    static tstring GetModulePath() { return strStartPath; }
    static void        SetModulePath(const tstring &strPath)
    {
        tstring::size_type Pos = strPath.find_last_of(_T("\\"), strPath.size());
        if (Pos != strPath.npos)
            strStartPath = strPath.substr(0, Pos);
        else
            strStartPath = _T(".");
    }

    static void		SetDefaultModulePath()
    {
        DWORD ret;
        TCHAR buf[MAX_PATH];
        ret = GetModuleFileName( NULL, buf, MAX_PATH );		// get name for current module
        if( ret == 0 )
        {
            FMsg(TEXT("SetDefaultModulePath() GetModuleFileName() failed!\n"));
            assert( false );
        }

        SetModulePath( buf );
    }

    //a helper class to save and restore the currentDirectory
    class DirectorySaver
    {
    private:
        TCHAR savedDirectory[MAX_PATH];
    public:
        DirectorySaver( )
        {
            // Save current directory
            GetCurrentDirectory(MAX_PATH, savedDirectory);
        }
        ~DirectorySaver( )
        {
            // return to previous directory
            SetCurrentDirectory(this->savedDirectory);
        }
    };

    // Recursively searchs the given path until it finds the file. Returns "" if 
    // file can't be found
    static tstring FindMediaFile(const tstring &strFilename, const tstring &mediaPath, bool bVerbose = false )
    {
        WIN32_FIND_DATA findInfo;
        HANDLE hFind;
        tstring result;


        //save and auto restore the current working directory
        DirectorySaver whenIGoOutOfScopeTheCurrentWorkingDirectoryWillBeRestored;

        if (!SetCurrentDirectory(mediaPath.data()))
        {
            // DWORD tmp2 = GetLastError();
            if( bVerbose )
            {	
                FMsg(TEXT("FindMediaFile Couldn't SetCurrentDirectory to [%s].  Returning empty string\n"), mediaPath.c_str() );
            }
            return _T("");
        }

        // check if file is in current directory
        FILE *fp;
        errno_t err = _wfopen_s(&fp, strFilename.data(), _T("r"));
        if (!err)
        {
            fclose(fp);
            return mediaPath + _T("\\") + strFilename;
        }
        else
        {	
            if( bVerbose )
            {
                // report where the file is NOT
                FMsg(TEXT("FindMediaFile: File [%s] is not in %s\n"), strFilename.c_str(), mediaPath.c_str() );
            }
        }

        // if file not in current directory, search for all directories
        // and search inside them until the file is found
        hFind = FindFirstFile( _T( "*.*" ) , &findInfo);
        if (hFind == INVALID_HANDLE_VALUE)
            return _T("");

        result = _T("");
        do
        {
            if (findInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                // Make sure we don't try to delete the '.' or '..' directories
                if ((_tcscmp(findInfo.cFileName, _T("..")) == 0) || (_tcscmp(findInfo.cFileName, _T(".")) == 0))
                    continue;

                // Add directory to the media path
                // Keep same original file name
                result = FindMediaFile(strFilename, mediaPath + _T("\\") + findInfo.cFileName, bVerbose );
                if (result != _T(""))
                    break;                    
            }
        } while (FindNextFile(hFind, &findInfo));

        FindClose(hFind);

        return result;
    }

    //////////////////////////////////////////////////////////////////////////////
    // Gets the full path to the requested file by searching in the MEDIA directory.
    // (note: for a pointer to a 1-parameter version of this function, see below.)
    static tstring GetFilePath(const tstring& strFileName, bool bVerbose )
    {
        // Check if strFilename already points to a file
        FILE *fp;
        errno_t err = _wfopen_s(&fp, strFileName.data(), _T("r"));
        if (!err)
        {
            fclose(fp);
            return strFileName;
        }

        // You should call SetModulePath before using GetFilePath()
        // If not, it will set the module path for you

        tstring strMediaPath = GetModulePath();
        if( strMediaPath.empty() == true )
        {
            SetDefaultModulePath();
            strMediaPath = GetModulePath();
        }

        // Search the path of the project
        strMediaPath = strMediaPath.substr(0, strMediaPath.find_last_of(_T("\\/")));
        strMediaPath = strMediaPath.substr(0, strMediaPath.find_last_of(_T("\\/")));
        strMediaPath = strMediaPath.substr(0, strMediaPath.find_last_of(_T("\\/")));

        tstring result;
        result = FindMediaFile(strFileName, strMediaPath, bVerbose);
        if (result != _T(""))
            return result;

        // Find the last occurrence of '\' or '/'
        // This has the effect of backing up 4 directories, implying a direct dependence on
        //  the location of the calling .exe in order to start looking for media in the right
        //  place.  This is bad, so a more general search for the first subdirectory \MEDIA 
        //  should be put in place
        // TODO : see above
        strMediaPath = strMediaPath.substr(0, strMediaPath.find_last_of(_T("\\/")));
        strMediaPath = strMediaPath.substr(0, strMediaPath.find_last_of(_T("\\/")));
        strMediaPath = strMediaPath.substr(0, strMediaPath.find_last_of(_T("\\/")));
        //	    strMediaPath = strMediaPath.substr(0, strMediaPath.find_last_of(_T("\\/")));
        strMediaPath += _T("\\Media");

        result = FindMediaFile(strFileName, strMediaPath, bVerbose);
        if (result != _T(""))
            return result;

        //////////////////// for local shaders /////////////////////////
        strMediaPath = GetModulePath();
        strMediaPath = strMediaPath.substr(0, strMediaPath.find_last_of(_T("\\/")));
        strMediaPath = strMediaPath.substr(0, strMediaPath.find_last_of(_T("\\/")));
        strMediaPath = strMediaPath.substr(0, strMediaPath.find_last_of(_T("\\/")));
        strMediaPath += _T("\\Shaders");

        result = FindMediaFile(strFileName, strMediaPath, bVerbose);

        if (result != _T(""))
            return result;

        //////////////////// for local ../shaders /////////////////////////
        strMediaPath = GetModulePath();
        strMediaPath = strMediaPath.substr(0, strMediaPath.find_last_of(_T("\\/")));
        strMediaPath += _T("\\Shaders");

        result = FindMediaFile(strFileName, strMediaPath, bVerbose);

        if (result != _T(""))
            return result;

        // If prog gets to here, the find has failed.
        // Return the input file name so other apps can report the failure
        //  to find the file.
        if( bVerbose )
            FMsg(TEXT("GetFilePath() Couldn't find : %s\n"), strFileName.c_str() );

        return strFileName;
    };

    // Use these wrapper functions if you need to pass a pointer [callback] 
    // to a 1-parameter version of the GetFilePath() function:
    static tstring GetFilePath(const tstring& strFileName) {
        return GetFilePath(strFileName, false);
    }
};


inline void createBufferAndUAV(ID3D11Device* pd3dDevice, void* data, UINT byte_width, UINT byte_stride,
                               ID3D11Buffer** ppBuffer, ID3D11UnorderedAccessView** ppUAV, ID3D11ShaderResourceView** ppSRV)
{
    // Create buffer
    D3D11_BUFFER_DESC buf_desc;
    buf_desc.ByteWidth = byte_width;
    buf_desc.Usage = D3D11_USAGE_DEFAULT;
    buf_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    buf_desc.CPUAccessFlags = 0;
    buf_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    buf_desc.StructureByteStride = byte_stride;

    D3D11_SUBRESOURCE_DATA init_data = {data, 0, 0};

    pd3dDevice->CreateBuffer(&buf_desc, data != NULL ? &init_data : NULL, ppBuffer);
    assert(*ppBuffer);

    // Create undordered access view
    D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
    uav_desc.Format = DXGI_FORMAT_UNKNOWN;
    uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uav_desc.Buffer.FirstElement = 0;
    uav_desc.Buffer.NumElements = byte_width / byte_stride;
    uav_desc.Buffer.Flags = 0;

    pd3dDevice->CreateUnorderedAccessView(*ppBuffer, &uav_desc, ppUAV);
    assert(*ppUAV);

    // Create shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
    srv_desc.Format = DXGI_FORMAT_UNKNOWN;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srv_desc.Buffer.FirstElement = 0;
    srv_desc.Buffer.NumElements = byte_width / byte_stride;

    pd3dDevice->CreateShaderResourceView(*ppBuffer, &srv_desc, ppSRV);
    assert(*ppSRV);
}


inline void createUAV(ID3D11Device* pd3dDevice, ID3D11Buffer * buf, UINT byte_width, UINT byte_stride,
                      ID3D11UnorderedAccessView** ppUAV, ID3D11ShaderResourceView** ppSRV)
{
    // Create undordered access view
    D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
    uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
    uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uav_desc.Buffer.FirstElement = 0;
    uav_desc.Buffer.NumElements = byte_width / 4;
    uav_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

    pd3dDevice->CreateUnorderedAccessView(buf, &uav_desc, ppUAV);
    assert(*ppUAV);

    // Create shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
    srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    srv_desc.BufferEx.FirstElement = 0;
    srv_desc.BufferEx.NumElements = byte_width / 4;
    srv_desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;

    pd3dDevice->CreateShaderResourceView(buf, &srv_desc, ppSRV);
    assert(*ppSRV);
}

inline void createTextureAndViews(ID3D11Device* pd3dDevice, UINT width, UINT height, DXGI_FORMAT format,
                                  ID3D11Texture2D** ppTex, ID3D11ShaderResourceView** ppSRV, ID3D11RenderTargetView** ppRTV)
{
    // Create 2D texture
    D3D11_TEXTURE2D_DESC tex_desc;
    tex_desc.Width = width;
    tex_desc.Height = height;
    tex_desc.MipLevels = 0;
    tex_desc.ArraySize = 1;
    tex_desc.Format = format;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.SampleDesc.Quality = 0;
    tex_desc.Usage = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    tex_desc.CPUAccessFlags = 0;
    tex_desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    pd3dDevice->CreateTexture2D(&tex_desc, NULL, ppTex);
    assert(*ppTex);

    // Create shader resource view
    (*ppTex)->GetDesc(&tex_desc);
    if (ppSRV)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
        srv_desc.Format = format;
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = tex_desc.MipLevels;
        srv_desc.Texture2D.MostDetailedMip = 0;

        pd3dDevice->CreateShaderResourceView(*ppTex, &srv_desc, ppSRV);
        assert(*ppSRV);
    }

    // Create render target view
    if (ppRTV)
    {
        D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
        rtv_desc.Format = format;
        rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtv_desc.Texture2D.MipSlice = 0;

        pd3dDevice->CreateRenderTargetView(*ppTex, &rtv_desc, ppRTV);
        assert(*ppRTV);
    }
}

#endif
