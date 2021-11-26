// These are type declarations for features used from the older v2 web UI.
// This file will hopefully reduce over time
declare function set_menu(pos1:string, pos2:string, user:string): void;
declare function clear_alert(): void;
declare function alertInvalidRequest(): void;
declare function success_alert(s1:string, s2:string): void;
declare function validate_alert(s1:string, s2:string): void;
declare function alertInvalidCsrfToken(): void;
declare function blockUI_alert(s:string): void;
declare function blockUI_alert_l(s:string, p?:any): void;
declare function blockUI_wait(s:string): void;
declare function blockUI_wait_progress(s:string): void;
declare function blockUI_wait_confirm(s:string, l:string, c:any): void;
declare function blockUI_confirm(s:string, y?:any, n?:any): void;
declare function lang_sentence(p1:string, p2: string[]): string;
declare var Base64: any;
declare function isHexaDigit(p:string) : boolean;
declare function isDigit(p:string) : boolean;
declare function isAllNum(p:string) : boolean;
declare function htmlNumberEncode(s : string) : string;
declare function convert_to_html_entity(s : string) : string;
declare function isValidIpAddress(p:string) : boolean;
declare function isValidIpv6Address(p:string) : boolean;
declare function isValidSubnetMask(p:string) : number;
declare function is_valid_domain_name(p:string) : boolean;
declare function hostNameFilter(s : any) : string;
declare function nameFilter(s : string) : string;
declare function NumfieldEntry(s : any) : string;
declare function SignedNumfieldEntry(s : any) : string;
declare function urlFilter(s : string) : string;
declare function emailFilter(s : string) : string;
declare function parse_ip_from_fields(s : string) : string;
declare function parse_ip_into_fields(s1 : string, s2 : string) : any;
declare function genHtmlIpBlocks(s : string) : string;
declare function genHtmlIpBlocksWithoutRequired(s : string) : string;
declare function genHtmlIpBlocks0(s : string) : string;
declare function genHtmlMaskBlocks(s : string) : string;
declare function maskBits(s: string): number;
declare function toUpTime(s: number): string;
declare function atoi(str, num): number;

declare var relUrlOfPage: string;
declare var pageData: PageDef;
declare var csrfToken: string;
declare var xlat: string[] | undefined;
declare var user: string;
declare var service_pppoe_server_0_enable: string;
declare var service_pppoe_server_0_wanipforward_enable: string;


interface Window {
  clipboardData: any
}

interface HTMLElement {
  files: any
}
