#include "crazy.h"
#include "crazy/rapidxml/rapidxml.h"
#include <iterator>

std::string g_xml_test = R"(
<ROOT>
   <CONFIG>
      <IP_NUMBER>192.168.249.123</IP_NUMBER>   <!-- IP-number of the external socket -->
      <PORT>49152</PORT>                   <!-- Port-number of the external socket -->      
      <SENTYPE>ImFree</SENTYPE>        <!-- The name of your system send in <Sen Type="" > -->     
      <ONLYSEND>FALSE</ONLYSEND>       <!-- TRUE means the client don't expect answers. Do not send anything to robot -->
   </CONFIG>
   <!-- RSI Data: TYPE=  "BOOL", "STRING", "LONG", "DOUBLE" -->
   <!-- INDX= "INTERNAL" switch on internal read values. Needed by DEF_... -->
   <!-- INDX= "nmb" Input/Output index of RSI-Object / Maximum of RSI Channels: 64  -->   
   <!-- HOLDON="1", set this output index of RSI Object to the last value  -->   
   <!-- DEF_Delay count the late packages and send it back to server  -->
   <!-- DEF_Tech: .C = advance .T = main run / .C1 advance set function generator 1 -->
   
   <SEND>
      <ELEMENTS>
         <ELEMENT TAG="DEF_RIst" TYPE="DOUBLE" INDX="INTERNAL" />
         <ELEMENT TAG="DEF_RSol" TYPE="DOUBLE" INDX="INTERNAL" />
         <ELEMENT TAG="DEF_AIPos" TYPE="DOUBLE" INDX="INTERNAL" />
         <ELEMENT TAG="DEF_ASPos" TYPE="DOUBLE" INDX="INTERNAL" />
         <ELEMENT TAG="DEF_Delay" TYPE="LONG" INDX="INTERNAL" />
      </ELEMENTS>
   </SEND>
   <RECEIVE>
      <ELEMENTS>
         <ELEMENT TAG="AK.A1" TYPE="DOUBLE" INDX="1" HOLDON="1" />
         <ELEMENT TAG="AK.A2" TYPE="DOUBLE" INDX="2" HOLDON="1" />
         <ELEMENT TAG="AK.A3" TYPE="DOUBLE" INDX="3" HOLDON="1" />
         <ELEMENT TAG="AK.A4" TYPE="DOUBLE" INDX="4" HOLDON="1" />
         <ELEMENT TAG="AK.A5" TYPE="DOUBLE" INDX="5" HOLDON="1" />
         <ELEMENT TAG="AK.A6" TYPE="DOUBLE" INDX="6" HOLDON="1" />
      </ELEMENTS>
   </RECEIVE>
</ROOT>
)";

int main() {
	rapidxml::xml_document<> doc;
	doc.parse<0>(g_xml_test.data());	
	rapidxml::xml_node<> *root = doc.first_node("ROOT");
	for (rapidxml::xml_node<> *node = root->first_node("CONFIG")->first_node(); node; node = node->next_sibling()) {
		if (node->first_node() != NULL) {
			std::cout << node->name() << " : " << node->value() << std::endl;
		}
			
		else
			std::cout << "name: " << node->name() << "has no value" << std::endl;
		for (rapidxml::xml_attribute<>* attribute = node->first_attribute(); attribute; attribute = attribute->next_attribute())
			std::cout << "attribute name = " << attribute->name() << "attribute value = " << attribute->value() << std::endl;
 
	}
 
	rapidxml::xml_node<> *send = root->first_node("SEND");
	for (rapidxml::xml_node<> *node = send->first_node("ELEMENTS")->first_node(); node; node = node->next_sibling()) {
		if (node->first_node() != NULL) {
			std::cout << node->name() << " : " << node->value() << std::endl;
		}
 
		else
			std::cout << "name: " << node->name() << "has no value" << std::endl;
		for (rapidxml::xml_attribute<>* attribute = node->first_attribute(); attribute; attribute = attribute->next_attribute())
			std::cout << "attribute name = " << attribute->name() << "attribute value = " << attribute->value() << std::endl;
 
	}
 
	rapidxml::xml_node<> *receive = root->first_node("RECEIVE");
	for (rapidxml::xml_node<> *node = receive->first_node("ELEMENTS")->first_node(); node; node = node->next_sibling()) {
		if (node->first_node() != NULL)
		{
			std::cout << node->name() << " : " << node->value() << std::endl;
		}
 
		else
			std::cout << "name: " << node->name() << "has no value" << std::endl;
		for (rapidxml::xml_attribute<>* attribute = node->first_attribute(); attribute; attribute = attribute->next_attribute())
			std::cout << "attribute name = " << attribute->name() << "attribute value = " << attribute->value() << std::endl;
	}

}
