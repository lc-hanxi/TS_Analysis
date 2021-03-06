#include "EIT.h"
#include "../../TSAnalysis/TSFactory.h"
#include "../../Descriptor/Descriptor.h"
#include "../../Descriptor/DVB/DescFactory.h"

EIT::EventInfo::EventInfo(uint8_t* data)
    : event_id((data[0] << 8) | data[1]),
      start_time(),
      duration(),
      running_status(data[10] >> 5),
      free_CA_mode((data[10] >> 4) & 0x01),
      descriptors_loop_length(((data[10] & 0x0F) << 8) | data[11])
{
    memcpy(start_time, data + 2, 5);
    memcpy(duration, data + 7, 3);

    int idx = 0;
    uint8_t* pd = data + 12;
    DescFactory des_fac;
    while(idx < descriptors_loop_length)
    {
        Descriptor* des = des_fac.createDesc(pd[idx], pd + idx);
        desc_list.push_back(des);
        idx += des->length + 2;
    }
}

EIT::EventInfo::~EventInfo()
{
    std::list<Descriptor*>::iterator dit;
    for(dit = desc_list.begin(); dit != desc_list.end(); ++dit)
    {
        delete (*dit);
    }
    desc_list.clear();
}

bool EIT::EventInfo::operator<(const EventInfo& et)
{
    return event_id < et.event_id;
}

EIT::EIT(uint8_t* data, uint16_t len, uint32_t crc)
    : Section(data, len),
      service_id((data[3] << 8) | data[4]), 
      version_number((data[5] >> 1) & 0x1F),
      current_next_indicator(data[5] >> 7),
      section_number(data[6]),
      last_section_number(data[7]),
      transport_stream_id((data[8] << 8) | data[9]),
      original_network_id((data[10] << 8) | data[11]),
      segment_last_section_number(data[12]),
      last_table_id(data[13]),
      event_list(),
      crc32((data[len - 4] << 24) | (data[len - 3] << 16) | (data[len - 2] << 8) | data[len - 1])
{
    if(crc != 0xFFFFFFFF)
    {
        if(crc != crc32)
            throw new CrcErr(CrcErr::CEIT);
    }
}

EIT::~EIT()
{
    std::set<EventInfo*, cmp_secp<EventInfo>>::iterator 
        eit = event_list.begin();
    for(; eit != event_list.end(); ++eit)
    {
        delete (*eit);
    }
    event_list.clear();
}

bool EIT::operator==(const EIT& et)
{
    return table_id == et.table_id &&
           transport_stream_id == et.transport_stream_id &&
           service_id == et.service_id &&
		   section_number == et.section_number;
}

bool EIT::operator<(const EIT& et)
{
    if(table_id < et.table_id)
        return true;
    else if(table_id == et.table_id)
    {
        if(transport_stream_id < et.transport_stream_id)
            return true;
        else if(transport_stream_id == et.transport_stream_id)
        {
            if(service_id < et.service_id)
                return true;
            else if(service_id == et.service_id)
            {
                if(section_number < et.section_number)
                    return true;
            }
        }
    }

    return false;
}

void EIT::getDetail()
{
    if(raw_data == NULL)
        return ;

    int index = 14;
    while(index < length - 1)
    {
        EventInfo* ei = new EventInfo(raw_data + index);
        event_list.insert(ei);
        index += 12 + ei->descriptors_loop_length;
    }

    delete []raw_data;
    raw_data = NULL;
}

bool EIT::joinTo(TSFactory* sf)
{
    std::set<EIT*, cmp_secp<EIT>>::iterator
        fit = sf->eit_list.find(this);

    if(fit == sf->eit_list.end())
    {
        sf->eit_list.insert(this);
        this->getDetail();
        return true;
    }
    else
    {
        if((*fit)->version_number < this->version_number)
        {
            EIT* et = (*fit);
            et->version_number = this->version_number;
            et->crc32 = this->crc32;
            return true;
        }
    }

    return false;
}

void EIT::resolved()
{
    TiXmlElement* tmp;
    char arr[32] = {0};

    Section::resolved();
    xml->SetAttribute("table_id", table_id);
    xml->SetAttribute("ts_id", transport_stream_id);
    xml->SetAttribute("serv_id", service_id);
    xml->SetAttribute("sect_num", section_number);
    xml->SetAttribute("ver_num", version_number);

    sprintf(arr, "0x%x", service_id);
    tmp = new TiXmlElement("service_id");
    tmp->LinkEndChild(new TiXmlText(arr));
    xml->LinkEndChild(tmp);

    sprintf(arr, "0x%x", version_number);
    tmp = new TiXmlElement("version_number");
    tmp->LinkEndChild(new TiXmlText(arr));
    xml->LinkEndChild(tmp);

    sprintf(arr, "0x%x", current_next_indicator);
    tmp = new TiXmlElement("current_next_indicator");
    tmp->LinkEndChild(new TiXmlText(arr));
    xml->LinkEndChild(tmp);

    sprintf(arr, "0x%x", section_number);
    tmp = new TiXmlElement("section_number");
    tmp->LinkEndChild(new TiXmlText(arr));
    xml->LinkEndChild(tmp);

    sprintf(arr, "0x%x", last_section_number);
    tmp = new TiXmlElement("last_section_number");
    tmp->LinkEndChild(new TiXmlText(arr));
    xml->LinkEndChild(tmp);

    sprintf(arr, "0x%x", transport_stream_id);
    tmp = new TiXmlElement("transport_stream_id");
    tmp->LinkEndChild(new TiXmlText(arr));
    xml->LinkEndChild(tmp);

    sprintf(arr, "0x%x", segment_last_section_number);
    tmp = new TiXmlElement("segment_last_section_number");
    tmp->LinkEndChild(new TiXmlText(arr));
    xml->LinkEndChild(tmp);

    sprintf(arr, "0x%x", last_table_id);
    tmp = new TiXmlElement("last_table_id");
    tmp->LinkEndChild(new TiXmlText(arr));
    xml->LinkEndChild(tmp);

    sprintf(arr, "0x%x", original_network_id);
    tmp = new TiXmlElement("original_network_id");
    tmp->LinkEndChild(new TiXmlText(arr));
    xml->LinkEndChild(tmp);

    if(!event_list.empty())
    {
        std::set<EventInfo*, cmp_secp<EventInfo>>::iterator 
            eit = event_list.begin();
        for(; eit != event_list.end(); ++eit)
        {
            tmp = new TiXmlElement("Event");
            TiXmlElement* tms;

            sprintf(arr, "0x%x", (*eit)->event_id);
            tms = new TiXmlElement("event_id");
            tms->LinkEndChild(new TiXmlText(arr));
            tmp->LinkEndChild(tms); 

            utc_to_ymdhms((*eit)->start_time, arr);
            tms = new TiXmlElement("start_time");
            tms->LinkEndChild(new TiXmlText(arr));
            tmp->LinkEndChild(tms); 

            utc_to_hms((*eit)->duration, arr);
            tms = new TiXmlElement("duration");
            tms->LinkEndChild(new TiXmlText(arr));
            tmp->LinkEndChild(tms);

            sprintf(arr, "0x%x", (*eit)->running_status);
            tms = new TiXmlElement("running_status");
            tms->LinkEndChild(new TiXmlText(arr));
            tmp->LinkEndChild(tms);

            sprintf(arr, "0x%x", (*eit)->free_CA_mode);
            tms = new TiXmlElement("free_CA_mode");
            tms->LinkEndChild(new TiXmlText(arr));
            tmp->LinkEndChild(tms);

            sprintf(arr, "0x%x", (*eit)->descriptors_loop_length);
            tms = new TiXmlElement("descriptors_loop_length");
            tms->LinkEndChild(new TiXmlText(arr));
            tmp->LinkEndChild(tms);

            if(!(*eit)->desc_list.empty())
            {
                tms = new TiXmlElement("Descriptors");
                std::list<Descriptor*>::iterator sdit;
                for(sdit = (*eit)->desc_list.begin(); sdit != (*eit)->desc_list.end(); ++sdit)
                {
                    (*sdit)->resolved();
                    tms->LinkEndChild((*sdit)->xml);
                }
                tmp->LinkEndChild(tms);
            } 

            xml->LinkEndChild(tmp);
        } 
    }

    sprintf(arr, "0x%x", crc32);
    tmp = new TiXmlElement("CRC32");
    tmp->LinkEndChild(new TiXmlText(arr));
    xml->LinkEndChild(tmp);
}
