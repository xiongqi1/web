function poll_eth_stat() {

    $.getJSON( "/cgi-bin/eth.cgi?cmd=info_eth_profile",
        function( data ) {
          var eth_profiles=data.eth_profiles;

          /* sort */
          eth_profiles.sort((a,b) => a.metrics-b.metrics);

          /* build failover status table */
          var div_ethwan_fo = [];
          eth_profiles.forEach(function(o){
            /* apply default */
            if(o.monitor_type == "")
              o.monitor_type = "phy";

            if(div_ethwan_fo.length == 0) {
              div_ethwan_fo.push("<dl>" + "<dt>" + _("priority") + "</dt>" + "</dl>");
            }

            div_ethwan_fo.push(
              "<dl>"+
                "<dd><span style='display:inline-block;width:140px'>" + (o.dev_type == "wwan" ? "wwan." + o.pf: o.dev) +
                " ["+_(o.monitor_type)+"]</span>" +
                "<i class=" + (o.fo_status=="up" ? "success-sml": "warning-sml") +
                "></i></dd>" +
              "</dl>"
            );
          });

          $("#StsWanInfo_0_0_div").html(div_ethwan_fo.length == 0? "<dl><dd>"+_("no wan interface is configured")+"</dd></dl>": div_ethwan_fo.join(""));

          setTimeout(poll_eth_stat,5000);
        }
      );
  #if 0
      /* build eth status table */
      var tbody_eth_wan=new Array();
      $.each(eth_profiles,function(i,o){

        if(o.dev_type!="eth")
          return;

        /* apply default */
        if(o.monitor_type=="")
          o.monitor_type="phy";

        if(tbody_eth_wan.length==0) {
          tbody_eth_wan.push(
            "<tr>"+
              "<th class='align10-2'>No</th>"+
              "<th class='align10-2'>"+_("name")+"</th>"+
              "<th class='align10-2'>"+_("status")+"</th>"+
              "<th class='align10-2'>"+_("ip address")+"</th>"+
              "<th class='align10-2'>"+_("netmask")+"</th>"+
              "<th class='align10-2'>"+_("gateway")+"</th>"+
            "</tr>"
          );
        }

        if(o.status!="up") {
          o.ip=_("na");
          o.mask=_("na");
          o.gw=_("na");
        }

        tbody_eth_wan.push(
          "<tr>"+
            "<td>"+i+"</td>"+
            "<td>"+o.dev+"</td>"+
            "<td>"+o.status+"</td>"+
            "<td>"+o.ip+"</td>"+
            "<td>"+o.mask+"</td>"+
            "<td>"+o.gw+"</td>"+
          "</tr>"
        );
      });
      $tbody_eth_wan.html(tbody_eth_wan.join());

      $("#eth-wan-div").toggle(tbody_eth_wan.length>0);
    });
#endif
}

var WanInfo = PageObj("StsWanInfo", "wan",
  {
    readOnly: true,
    column: 2,
    genObjHtml: genCols,
    columns : [{
      members:[
        {hdg: "IP"}
      ]
    }],

    populate: function () { setTimeout(poll_eth_stat,1000); return _populateCols(this.columns, this.obj)},

    members: [
    ]
  }
);

stsPageObjects.push(WanInfo);
