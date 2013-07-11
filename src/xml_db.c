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


#define     APPLIANCE         "ZIGTOIR"           //家电

#define     CMD_NODE    "CMD"

#define     VERSION     "version"

int xmldb_open(void)
{
    FILE * fp;
    mxml_node_t * version;
    fp = fopen(XML_DB, "r");        //打开文件

    if (fp != NULL) {        //文件不存在 创建
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

    } else if (fp == NULL && errno == EEXIST) { //文件存在 读取
        xml_root = mxmlLoadFile(NULL, fp, MXML_TEXT_CALLBACK); //已子节点载入 全部使用字符串
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

int xml_add_fun()                   //添加功能模块
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

        xml_area = mxmlFindElement(xml, xml, area, NULL, NULL, MXML_DESCEND_FIRST);//超找area区域
        if (xml_area == NULL)
            break;
        xml_node = mxmlFindElement(xml_area, xml, node, NULL, NULL, MXML_DESCEND_FIRST);//查看是否有node节点
        if (xml_area == NULL) { //没有此节点
            ret = 0;
            break;
        }
        mxmlDelete(xml_node);
        ret = 0;
    } while (0);

    XML_UNLOCK();
}


                      //区域编号  节点编号      功能编号   （家电名称          家电类型  (空调等等 没有均为NULL))
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

        xml_area = mxmlFindElement(xml, xml, area, NULL, NULL, MXML_DESCEND_FIRST);//超找area区域
        if (xml_area == NULL)
            break;




        xml_node = mxmlFindElement(xml_area, xml, node, NULL, NULL, MXML_DESCEND_FIRST);//查看是否有node节点
        if (xml_node == NULL) { //没有找到添加
            xml_node = mxmlNewElement(xml_area, node);      //添加node节点
            if (xml_node == NULL)
                break;
            ret = 0;
        }

        if (fun == NULL)
            break;
        //找到节点
        xml_fun = mxmlFindElement(xml_node, xml, fun, NULL, NULL , MXML_DESCEND_FIRST);//查看是否已经有了
        if (xml_fun == NULL) {  //没有此功能号
            xml_fun = mxmlNewElement(xml_node, fun);       //添加fun功能结合节点
            if (xml_fun == NULL) {
                ret = -1;
                break;
            }
            ret = 0;
        }

        if (app == NULL || app_type == NULL)
            break;

        pelm = mxmlGetElement(xml_fun);

        if (!strcmp(pelm, APPLIANCE)) {                 //名称匹配 是否是zigbee 转红外模块
            //家电
            xml_app = mxmlFindElement(xml_fun, xml, fun, NULL, NULL , MXML_DESCEND_FIRST);//查找app节点
            if (xml_app == NULL) {  //节点不存在则创建节点
                xml_app = mxmlNewElement(xml_fun, app);
                if (xml_app == NULL) {
                    ret = -1;
                    break;
                }
            }
            xml_app_type = mxmlFindElement(xml_app, xml, app_type, NULL, NULL, MXML_DESCEND_FIRST);//查找app_type节点
            if (xml_app_type == NULL) {         //节点不存在创建节点
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







