#pragma once

#include <map>
#include "Package.h"

enum PackageType
{
    SIGNIN,         // ��½
    SIGNIN_RES,     // ��½���ؽ��

    SIGNUP,         // ע��
    SIGNUP_RES,     // ע�᷵�ؽ��

    UPLOAD_REQ,     // �ϴ��ļ��ź� ��һ�θ�֪�������ļ�·������С������
    UPLOAD_RESP,    // �ϴ��ļ��Ľ��
    UPLOAD_FETCH,   // ��������Ҫ���ϴ�ĳһ�����ļ���
    UPLOAD_PUSH,    // �ͻ��˴�������������Ҫ�����ݿ�
 
    DOWNLOAD_REQ,   // �����ļ����ź� ��һ�θ�֪��������Ҫ���ص��ļ���·��������
    DOWNLOAD_RESP,  // �����ļ��Ľ��
    DOWNLOAD_PUSH,  // �������˸��ͻ���ĳһ����

    SYN,            // ͬ���ź�
    MKDIR,          // �½��ļ���
};

// ���ڲ�ָͬ����Ĵ�С��ӳ��� const���� �����Ը��� ֻ�ܳ�ʼ����ֵ
const static std::map<PackageType, uint16_t> PackageSizeMap{
    {PackageType::SIGNIN, sizeof(SigninBody)},
    {PackageType::SIGNIN_RES, sizeof(SigninresBody)},

    {PackageType::SIGNUP, sizeof(SignupBody)},
    {PackageType::SIGNUP_RES, sizeof(SignupresBody)},

    {PackageType::UPLOAD_REQ, sizeof(UploadReqBody)},
    {PackageType::UPLOAD_FETCH, sizeof(UploadFetchBody)},
    {PackageType::UPLOAD_PUSH, sizeof(UploadPushBody)},
    

};

// ͳһ��ͷ�� ÿ��ָ���ǰ��Ҫ�е�
struct UniformHeader
{
    PackageType p;
    uint16_t len;
    UniformHeader(const PackageType &pIn);
};

UniformHeader::UniformHeader(const PackageType &pIn) : p(pIn)
{
    // �����Ĵ�С
    len = PackageSizeMap.at(p);
}
