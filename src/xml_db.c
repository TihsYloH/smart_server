#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mxml.h"
#include "xml_db.h"
#include "log.h"
#include "buffer.h"

#define THREAD_SAFE_
#define XML_DB    "/mnt/xml_db_exist"
static mxml_node_t xml_root;

#ifdef THREAD_SAFE_
pthread_mutex_lock lock = PTHREAD_MUTEX_INITIALIZER;
#define XML_LOCK()  pthread_mutex_lock(&lock);
#define XML_UNLOCK() pthread_mutex_unlock(&lock);
#else
#define XML_LOCK()
#define XML_UNLOCK()

#endif


#define     APPLIANCE         "ZIGTOIR"           //�ҵ�

#define     CMD_NODE    "CMD"

#define     VERSION     "version"

int xmldb_open(void)
{
    FILE * fp;
    mxml_node_t * version;
    fp = fopen(XML_DB, "r");        //���ļ�

    if (fp != NULL) {        //�ļ������� ����
        mxml_node_t = mxmlNewXML("1.0");
        if (xml_root == NULL) {
            log_error(LOG_ERROR, "mxmlNewXML");
            return -1;
        }

        version = mxmlNewElement(xml_root, VERSION);
        if (version == NULL) {
            log_error(LOG_ERROR, "mxmlNewElement");
            return -1;
        }

        version = mxmlNewElement(xml_root, "0.00");

        if (version == NULL) {
            log_error(LOG_ERROR, "mxmlNewElement");
            return -1;
        }

    } else if (fp == NULL && errno == EEXIST) { //�ļ����� ��ȡ
        xml_root = mxmlLoadFile(NULL, fp, MXML_TEXT_CALLBACK); //���ӽڵ����� ȫ��ʹ���ַ���
        if (xml_root == NULL) {
            log_error(LOG_ERROR, "mxmlLoadFile");
            return -1;
        }
        /*
        cmd = mxmlNewElement(xml_root, CMD_NODE);
        if (cmd == NULL) {
            log_error(LOG_ERROR, "mxmlNewElement");
            return -1;
        }
        */
    } else {
        log_error(LOG_ERROR, "xml open");
        return -1;
    }
    return 0;
}


int xmldb_close(void)
{
    do {
        fp = fopen(XML_DB, "w");
    } while (fp == NULL && errno == EINTR);
    if (fp == NULL) {
        log_error(LOG_ERROR, "xmldb_close");
        return -1;
    }
    mxmlSaveFile(xml_root, fp, MXML_TEXT_CALLBACK);
    fclose(fp);
    return 0;
}

/*
int xml_addnodes(uint16_t area, uint64_t ext_addr, uint8_t mod)
{
    char buffer[100];
    char ext_buf[10];
    mxml_node_t * node;
    mxml_node_t * xml = xml_root;

    uint64tostr(ext_addr, ext_buf);

    snprintf(buffer, 100, "%s %04d %02d", ext_buf, area, mod);

    XML_LOCK();
    do {
        node = mxmlFindElement(xml, xml, buffer, NULL, NULL, MXML_DESCEND_FIRST);
        if (node == NULL) {
            node = mxmlNewElement(xml, buffer);
            if (node == NULL) {
                log_error(LOG_DEBUG, "add node error");
                ret = -1;
                break;
            }
        }
        ret = 0;
    } while (0);
    XML_UNLOCK();
    return ret;
}

int xml_delnodes(uint64_t ext_addr)
{
    char buffer[100];
    char ext_buf[10];
    mxml_node_t * node;
    mxml_node_t * xml = xml_root;

    uint64tostr(ext_addr, ext_buf);

    snprintf(buffer, 100, "%s %04d %02d", ext_buf, area, mod);

    XML_LOCK();
    do {
        node = mxmlFindElement(xml, xml, buffer, NULL, NULL, MXML_DESCEND_FIRST);
        if (node == NULL) {
            node = mxmlNewElement(xml, buffer);
            if (node == NULL) {
                log_error(LOG_DEBUG, "add node error");
                ret = -1;
                break;
            }
        }
        ret = 0;
    } while (0);
    XML_UNLOCK();
    return ret;
}
*/
int xml_add_area(char * area)
{
    int ret = 0;
    int len;
    mxml_node_t * xml = xml_root;
    mxml_node_t * node;

    len = strlen(area);
    if (len >= AREA_SIZ)
        return -1;

    XML_LOCK();
    do {
        node = mxmlFindElement(xml, xml, area, NULL, NULL, MXML_DESCEND_FIRST);
        if (node == NULL) {
            node = mxmlNewElement(xml, area);
            if (node == NULL) {
                ret = -1;
                break;
            }
            ret = 0;
        }
        ret = 0;
    } while (0);
    XML_UNLOCK();
    return ret;
}

int xml_del_area(char * area)
{
    int ret = 0;
    int len;
    mxml_node_t * xml = xml_root;
    mxml_node_t * node;

    len = strlen(area);
    if (len >= AREA_SIZ)
        return -1;

    XML_LOCK();
    do {
        node = mxmlFindElement(xml, xml, area, NULL, NULL, MXML_DESCEND_FIRST);
        if (node == NULL) {
            ret = -1;
            break;
        }
        mxmlDelete(node);
    } while (0);
    XML_UNLOCK();
    return ret;
}

int xml_add_fun()                   //��ӹ���ģ��
{
    return 0;
}

itn xml_del_fun()
{
    return 0;
}

int xml_del_node(char * area, char *node)
{
    mxml_node_t * xml = xml_root;
    mxml_node_t * xml_area;
    mxml_node_t * xml_node;

    int ret = -1;

    XML_LOCK();
    do {

        xml_area = mxmlFindElement(xml, xml, area, NULL, NULL, MXML_DESCEND_FIRST);//����area����
        if (xml_area == NULL)
            break;
        xml_node = mxmlFindElement(xml_area, xml, node, NULL, NULL, MXML_DESCEND_FIRST);//�鿴�Ƿ���node�ڵ�
        if (xml_area == NULL) { //û�д˽ڵ�
            ret = 0;
            break;
        }
        mxmlDelete(xml_node);
        ret = 0;
    } while (0);

    XML_UNLOCK();
}


                      //������  �ڵ���      ���ܱ��   ���ҵ�����          �ҵ�����  (�յ��ȵ� û�о�ΪNULL))
int xml_add_node(char * area, char *node, char * fun, char *app, char * app_type)
{
    mxml_node_t * xml = xml_root;
    mxml_node_t * xml_area;
    mxml_node_t * xml_node;
    mxml_node_t * xml_fun;
    mxml_node_t * xml_app;
    mxml_node_t * xml_app_type;
    const char * pelm;
    int ret = -1;

    XML_LOCK();
    do {

        xml_area = mxmlFindElement(xml, xml, area, NULL, NULL, MXML_DESCEND_FIRST);//����area����
        if (xml_area == NULL)
            break;




        xml_node = mxmlFindElement(xml_area, xml, node, NULL, NULL, MXML_DESCEND_FIRST);//�鿴�Ƿ���node�ڵ�
        if (xml_node == NULL) { //û���ҵ����
            xml_node = mxmlNewElement(xml_area, node);      //���node�ڵ�
            if (xml_node == NULL)
                break;
            ret = 0;
        }

        if (fun == NULL)
            break;
        //�ҵ��ڵ�
        xml_fun = mxmlFindElement(xml_node, xml, fun, NULL, NULL , MXML_DESCEND_FIRST);//�鿴�Ƿ��Ѿ�����
        if (xml_fun == NULL) {  //û�д˹��ܺ�
            xml_fun = mxmlNewElement(xml_node, fun);       //���fun���ܽ�Ͻڵ�
            if (xml_fun == NULL) {
                ret = -1;
                break;
            }
            ret = 0;
        }

        if (app == NULL || app_type == NULL)
            break;

        pelm = mxmlGetElement(xml_fun);

        if (!strcmp(pelm, APPLIANCE)) {                 //����ƥ�� �Ƿ���zigbee ת����ģ��
            //�ҵ�
            xml_app = mxmlFindElement(xml_fun, xml, fun, NULL, NULL , MXML_DESCEND_FIRST);//����app�ڵ�
            if (xml_app == NULL) {  //�ڵ㲻�����򴴽��ڵ�
                xml_app = mxmlNewElement(xml_fun, app);
                if (xml_app == NULL) {
                    ret = -1;
                    break;
                }
            }
            xml_app_type = mxmlFindElement(xml_app, xml, app_type, NULL, NULL, MXML_DESCEND_FIRST);//����app_type�ڵ�
            if (xml_app_type == NULL) {         //�ڵ㲻���ڴ����ڵ�
                xml_app_type = mxmlNewElement(xml_fun, app_type);
                if (xml_app_type == NULL)
                    ret = -1;
                break;
            }
        }
    } while (0);

    XML_UNLOCK();
    return ret;
}







