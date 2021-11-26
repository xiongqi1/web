local tabdef_msg = {
["registrationRequest"] = {
   {
    id    = "registrationRequest",
    reqd  = "Required",
    isray = true,
    tdef  = "registrationRequest",
        --[[ Array of RegistrationRequest data objects.
           --  Each RegistrationRequest data object
           --  represents a registration request of a
           --  CBSD.]]
   },
},

["registrationResponse"] = {
   {
    id    = "registrationResponse",
    reqd  = "Required",
    isray = true,
    tdef  = "registrationResponse",
        --[[ This parameter is an array of
           --  RegistrationResponse data objects. Each
           --  RegistrationResponse data object
           --  represents a registration response to a
           --  registration request from a CBSD.]]
   },
},

["spectrumInquiryRequest"] = {
   {
    id    = "spectrumInquiryRequest",
    reqd  = "Required",
    isray = true,
    tdef  = "spectrumInquiryRequest",
        --[[ Array of SpectrumInquiryRequest objects.
           --  Each SpectrumInquiryRequest object
           --  represents a spectrum inquiry request of
           --  a CBSD.]]
   },
},

["spectrumInquiryResponse"] = {
   {
    id    = "spectrumInquiryResponse",
    reqd  = "Required",
    isray = true,
    tdef  = "spectrumInquiryResponse",
        --[[ Array of SpectrumInquiryResponse objects.
           --  Each SpectrumInquiryResponse object
           --  represents a spectrum inquiry response
           --  to a spectrum inquiry request of a CBSD.]]
   },
},

["grantRequest"] = {
   {
    id    = "grantRequest",
    reqd  = "Required",
    isray = true,
    tdef  = "grantRequest",
        --[[ Array of GrantRequest objects. Each
           --  GrantRequest object represents a Grant
           --  request of a CBSD.]]
   },
},

["grantResponse"] = {
   {
    id    = "grantResponse",
    reqd  = "Required",
    isray = true,
    tdef  = "grantResponse",
        --[[ Array of GrantResponse objects. Each
           --  GrantResponse object represents a Grant
           --  response to a Grant request of a CBSD.]]
   },
},

["heartbeatRequest"] = {
   {
    id    = "heartbeatRequest",
    reqd  = "Required",
    isray = true,
    tdef  = "heartbeatRequest",
        --[[ Array of HeartbeatRequest objects. Each
           --  HeartbeatRequest object represents a
           --  heartbeat request of a CBSD.]]
   },
},

["heartbeatResponse"] = {
   {
    id    = "heartbeatResponse",
    reqd  = "Required",
    isray = true,
    tdef  = "heartbeatResponse",
        --[[ Array of HeartbeatResponse objects. Each
           --  HeartbeatResponse object represents a
           --  heartbeat response of a CBSD.]]
   },
},

["relinquishmentRequest"] = {
   {
    id    = "relinquishmentRequest",
    reqd  = "Required",
    isray = true,
    tdef  = "relinquishmentRequest",
   },
},

["relinquishmentResponse"] = {
   {
    id    = "relinquishmentResponse",
    reqd  = "Required",
    isray = true,
    tdef  = "relinquishmentResponse",
        --[[ Array of RelinquishmentResponse objects.
           --  Each RelinquishmentResponse object
           --  represents a relinquishment response to
           --  a relinquishment request of a CBSD.]]
   },
},

["deregistrationRequest"] = {
   {
    id    = "deregistrationRequest",
    reqd  = "Required",
    isray = true,
    tdef  = "deregistrationRequest",
        --[[ Array of DeregistrationRequest data
           --  objects. Each DeregistrationRequest data
           --  object represents a deregistration
           --  request of a CBSD.]]
   },
},

["deregistrationResponse"] = {
   {
    id    = "deregistrationResponse",
    reqd  = "Required",
    isray = true,
    tdef  = "deregistrationResponse",
        --[[ Array of DeregistrationResponse data
           --  objects. Each DeregistrationResponse
           --  data object represents a deregistration
           --  response to a deregistration request of
           --  a CBSD.]]
   },
},

}
local tabdef_obj = {
["registrationRequest"] = {
   {
    id    = "userId",
    reqd  = "Required",
    tdef  = "string",
        --[[ The UR-ID per [n.12] R2-SRR-02 conformant
           --  per section 2.2 of [n.18].]]
   },
   {
    id    = "fccId",
    reqd  = "Required",
    tdef  = "string",
        --[[ The FCC certification identifier of the
           --  CBSD. It is a string of up to 19
           --  characters as described in [n.13] and
           --  [n.15].]]
   },
   {
    id    = "cbsdSerialNumber",
    reqd  = "Required",
    tdef  = "string",
        --[[ A serial number assigned to the CBSD by
           --  the CBSD device manufacturer having a
           --  maximum length of 64 octets.  This
           --  serial number shall be unique for every
           --  CBSD instance sharing the same value of
           --  fccId. Each CBSD has a single CBSD
           --  Antenna (Ref. definition in section 4)
           --  and has a single cbsdSerialNumber.]]
   },
   {
    id    = "callSign",
    reqd  = "Optional",
    tdef  = "string",
        --[[ A device identifier provided by the FCC
           --  per [i.5]. NOTE: This parameter is for
           --  further study.]]
   },
   {
    id    = "cbsdCategory",
    reqd  = "REG-Conditional",
    tdef  = "string",
        --[[ Device Category of the CBSD. Allowed
           --  values are “A” or “B” as defined
           --  in Part 96. See “REG-Conditional
           --  Registration Request Parameters”
           --  above.]]
   },
   {
    id    = "cbsdInfo",
    reqd  = "Optional",
    tdef  = "cbsdInfo",
        --[[ Information about this CBSD model.]]
   },
   {
    id    = "airInterface",
    reqd  = "REG-Conditional",
    tdef  = "airInterface",
        --[[ A data object that includes information on
           --  the air interface technology of the
           --  CBSD. See “REG-Conditional
           --  Registration Request Parameters”
           --  above.]]
   },
   {
    id    = "installationParam",
    reqd  = "REG-Conditional",
    tdef  = "installationParam",
        --[[ A data object that includes information on
           --  CBSD installation. See
           --  “REG-Conditional Registration Request
           --  Parameters” above.]]
   },
   {
    id    = "measCapability",
    reqd  = "REG-Conditional",
    isray = true,
    tdef  = "array of string",
        --[[ The array of string lists measurement
           --  reporting capabilities of the CBSD. The
           --  permitted enumerations are specified in
           --  [n.21]. See “REG-Conditional
           --  Registration Request Parameters”
           --  above.]]
   },
   {
    id    = "groupingParam",
    reqd  = "Optional",
    tdef  = "jsonstring",
        --[[ An array of data objects that includes
           --  information on CBSD grouping.]]
   },
   {
    id    = "cpiSignatureData",
    reqd  = "Optional",
    tdef  = "jsonstring",
        --[[ The CPI is vouching for the parameters
           --  included in this object.  In addition,
           --  the digital signature for these
           --  parameters is included.]]
   },
},

["airInterface"] = {
   {
    id    = "radioTechnology",
    reqd  = "REG-Conditional",
    tdef  = "string",
        --[[ This parameter specifies the radio access
           --  technology that the CBSD uses for
           --  operation in the CBRS band. The
           --  permitted values are specified in
           --  [n.21]. See “REG-Conditional
           --  Registration Request Parameters”
           --  above.]]
   },
},

["installationParam"] = {
   {
    id    = "latitude",
    reqd  = "REG-Conditional",
    tdef  = "number",
        --[[ Latitude of the CBSD antenna location in
           --  degrees relative to the WGS 84 datum
           --  [n.11]. The allowed range is from -
           --  90.000000 to +90.000000.  Positive
           --  values represent latitudes north of the
           --  equator; negative values south of the
           --  equator.  Values are specified using 6
           --  digits to the right of the decimal
           --  point. Note: Use of WGS84 will also
           --  satisfy the NAD83 positioning
           --  requirements for CBSDs with the accuracy
           --  specified by Part 96 [n.8]. For
           --  reporting the CBSD location to the FCC,
           --  the SAS is responsible for converting
           --  coordinates from the WGS84 datum to the
           --  NAD83 datum. See “REG-Conditional
           --  Registration Request Parameters”
           --  above.]]
   },
   {
    id    = "longitude",
    reqd  = "REG-Conditional",
    tdef  = "number",
        --[[ Longitude of the CBSD antenna location in
           --  degrees relative to the WGS84 datum
           --  [n.11]. The allowed range is from -
           --  180.000000 to +180.000000.  Positive
           --  values represent longitudes east of the
           --  prime meridian; negative values west of
           --  the prime meridian.  Values are
           --  specified using 6 digits to the right of
           --  the decimal point. Note: Use of WGS84
           --  will also satisfy the NAD83 positioning
           --  requirements for CBSDs with the accuracy
           --  specified by Part 96 [n.8]. For
           --  reporting the CBSD location to the FCC,
           --  the SAS is responsible for converting
           --  coordinates from the WGS84 datum to the
           --  NAD83 datum. See “REG-Conditional
           --  Registration Request Parameters”
           --  above.]]
   },
   {
    id    = "height",
    reqd  = "REG-Conditional",
    tdef  = "number",
        --[[ The CBSD antenna height in meters. When
           --  the heightType parameter value is
           --  “AGL”, the antenna height should be
           --  given relative to ground level. When the
           --  heightType parameter value is
           --  “AMSL”, it is given with respect to
           --  WGS84 datum. For reporting the CBSD
           --  location to the FCC, the SAS is
           --  responsible for converting coordinates
           --  from the WGS84 datum to the NAD83 datum.
           --  See “REG-Conditional Registration
           --  Request Parameters” above.]]
   },
   {
    id    = "heightType",
    reqd  = "REG-Conditional",
    tdef  = "string",
        --[[ The value should be “AGL” or
           --  “AMSL”. AGL height is measured
           --  relative to the ground level. AMSL
           --  height is measured relative to the mean
           --  sea level. See “REG-Conditional
           --  Registration Request Parameters”
           --  above.]]
   },
   {
    id    = "horizontalAccuracy",
    reqd  = "Optional",
    tdef  = "number",
        --[[ A positive number in meters to indicate
           --  accuracy of the CBSD antenna horizontal
           --  location. This optional parameter should
           --  only be present if its value is less
           --  than the FCC requirement of 50 meters.]]
   },
   {
    id    = "verticalAccuracy",
    reqd  = "Optional",
    tdef  = "number",
        --[[ A positive number in meters to indicate
           --  accuracy of the CBSD antenna vertical
           --  location. This optional parameter should
           --  only be present if its value is less
           --  than the FCC requirement of 3 meters.]]
   },
   {
    id    = "indoorDeployment",
    reqd  = "REG-Conditional",
    tdef  = "boolean",
        --[[ Whether the CBSD antenna is indoor or not.
           --  True: indoor. False: outdoor. See
           --  “REG-Conditional Registration Request
           --  Parameters” above.]]
   },
   {
    id    = "antennaAzimuth",
    reqd  = "REG-Conditional",
    tdef  = "number",
        --[[ Boresight direction of the horizontal
           --  plane of the antenna in degrees with
           --  respect to true north. The value of this
           --  parameter is an integer with a value
           --  between 0 and 359 inclusive.  A value of
           --  0 degrees means true north; a value of
           --  90 degrees means east. This parameter is
           --  optional for Category A CBSDs and
           --  REG-conditional for Category B CBSDs.
           --  See “REG-Conditional Registration
           --  Request Parameters” above.]]
   },
   {
    id    = "antennaDowntilt",
    reqd  = "REG-Conditional",
    tdef  = "number",
        --[[ Antenna down tilt in degrees and is an
           --  integer with a value between -90 and +90
           --  inclusive; a negative value means the
           --  antenna is tilted up (above horizontal).
           --  This parameter is optional for Category
           --  A CBSDs and REG-conditional for Category
           --  B CBSDs. See “REG-Conditional
           --  Registration Request Parameters”
           --  above.]]
   },
   {
    id    = "antennaGain",
    reqd  = "REG-Conditional",
    tdef  = "number",
        --[[ Peak antenna gain in dBi. This parameter
           --  is an integer with a value between -127
           --  and +128 (dBi) inclusive. See
           --  “REG-Conditional Registration Request
           --  Parameters” above.]]
   },
   {
    id    = "eirpCapability",
    reqd  = "Optional",
    tdef  = "number",
        --[[ This parameter is the maximum CBSD EIRP in
           --  units of dBm/10MHz and is an integer
           --  with a value between -127 and +47
           --  (dBm/10MHz) inclusive. If not included,
           --  SAS interprets it as maximum allowable
           --  EIRP in units of dBm/10MHz for CBSD
           --  category.]]
   },
   {
    id    = "antennaBeamwidth",
    reqd  = "REG-Conditional",
    tdef  = "number",
        --[[ 3-dB antenna beamwidth of the antenna in
           --  the horizontal-plane in degrees. This
           --  parameter is an unsigned integer having
           --  a value between 0 and 360 (degrees)
           --  inclusive; it is optional for Category A
           --  CBSDs and REG-conditional for category B
           --  CBSDs. Note: A value of 360 (degrees)
           --  means the antenna has an omnidirectional
           --  radiation pattern in the horizontal
           --  plane. See “REG-Conditional
           --  Registration Request Parameters”
           --  above.]]
   },
   {
    id    = "antennaModel",
    reqd  = "Optional",
    tdef  = "string",
        --[[ If an external antenna is used, the
           --  antenna model is optionally provided in
           --  this field. The string has a maximum
           --  length of 128 octets.]]
   },
},

["groupParam"] = {
   {
    id    = "groupType",
    reqd  = "Required",
    tdef  = "string",
        --[[ Enumeration field describing the type of
           --  group this group ID describes. The
           --  following are permitted enumerations:
           --  INTERFERENCE_COORDINATION. Note:
           --  Additional group types are expected to
           --  be defined in future revisions of this
           --  specification.]]
   },
   {
    id    = "groupId",
    reqd  = "Required",
    tdef  = "string",
        --[[ This field specifies the identifier for
           --  this group of CBSDs. When the groupType
           --  is set to INTERFERENCE_COORDINATION, the
           --  namespace for groupId is userId.]]
   },
},

["cbsdInfo"] = {
   {
    id    = "vendor",
    reqd  = "Optional",
    tdef  = "string",
        --[[ The name of the CBSD vendor. The maximum
           --  length of this string is 64 octets.]]
   },
   {
    id    = "model",
    reqd  = "Optional",
    tdef  = "string",
        --[[ The name of the CBSD model. The maximum
           --  length of this string is 64 octets.]]
   },
   {
    id    = "softwareVersion",
    reqd  = "Optional",
    tdef  = "string",
        --[[ Software version of this CBSD. The maximum
           --  length of this string is 64 octets.]]
   },
   {
    id    = "hardwareVersion",
    reqd  = "Optional",
    tdef  = "string",
        --[[ Hardware version of this CBSD. The maximum
           --  length of this string is 64 octets.]]
   },
   {
    id    = "firmwareVersion",
    reqd  = "Optional",
    tdef  = "string",
        --[[ Firmware version of this CBSD. The maximum
           --  length of this string is 64 octets.]]
   },
},

["cpiSignatureData"] = {
   {
    id    = "protectedHeader",
    reqd  = "Required",
    tdef  = "string",
        --[[ The value of this parameter is the
           --  BASE64-encoded JOSE protected header.
           --  This is a JSON object equivalent to the
           --  JWT RS256 method or the ES256 method
           --  described in RFC 7515 [n.19] Section 3.
           --  BASE64 encoding is per RFC 4648 (see
           --  [n.20]). Valid values are equivalent to
           --  the JSON: { “typ”: “JWT”,
           --  “alg”: “RS256” } or { “typ”:
           --  “JWT”, “alg”: “ES256” }]]
   },
   {
    id    = "cpiSignedData",
    reqd  = "Required",
    tdef  = "string",
        --[[ The value of this parameter is the encoded
           --  JOSE payload data to be signed by the
           --  CPI’s private key. This parameter is
           --  calculated by taking the BASE64 encoding
           --  of a CpiSignedData object (see Table 10)
           --  according to the procedures in RFC 7515
           --  [n.19], Section 3.]]
   },
   {
    id    = "digitalSignature",
    reqd  = "Required",
    tdef  = "string",
        --[[ The value of this parameter is the CPI
           --  digital signature applied to the
           --  encodedCpiSignedData field. This
           --  signature is the BASE64URL encoding of
           --  the digital signature, prepared
           --  according to the procedures in RFC 7515
           --  [n.19] Section 3, using the algorithm as
           --  declared in the protectedHeader field.]]
   },
},

["cpiSignedData"] = {
   {
    id    = "fccId",
    reqd  = "Required",
    tdef  = "string",
        --[[ The value of this parameter is the FCC ID
           --  of the CBSD. Shall be equal to the fccId
           --  parameter value in the enclosing
           --  registration request.]]
   },
   {
    id    = "cbsdSerialNumber",
    reqd  = "Required",
    tdef  = "string",
        --[[ The value of this parameter is the CBSD
           --  serial number. Shall be equal to the
           --  cbsdSerialNumber of the enclosing
           --  registration request.]]
   },
   {
    id    = "installationParam",
    reqd  = "Required",
    tdef  = "installationParam",
        --[[ The value of this parameter is the
           --  InstallationParam object containing the
           --  parameters being certified by the CPI,
           --  and only those.]]
   },
   {
    id    = "professionalInstallerData",
    reqd  = "Required",
    tdef  = "professionalInstallerData",
        --[[ The value of this parameter is the data
           --  identifying the CPI vouching for the
           --  installation parameters included in the
           --  installationParam value contained in
           --  this object.]]
   },
},

["professionalInstallerData"] = {
   {
    id    = "cpiId",
    reqd  = "Required",
    tdef  = "string",
        --[[ The value of this parameter is the ID of
           --  the CPI providing information to the
           --  SAS. This string has a maximum length of
           --  256 octets.]]
   },
   {
    id    = "cpiName",
    reqd  = "Required",
    tdef  = "string",
        --[[ The value of this parameter is the human-
           --  readable name of the CPI providing
           --  information to the SAS.  This string has
           --  a maximum length of 256 octets.]]
   },
   {
    id    = "installCertificationTime",
    reqd  = "Required",
    tdef  = "string",
        --[[ The value of this parameter is the UTC
           --  date and time at which the CPI
           --  identified in this object certified the
           --  CBSD’s installed parameters. It is
           --  expressed using the format,
           --  YYYY-MM-DDThh:mm:ssZ, as defined by
           --  [n.7].]]
   },
},

["registrationResponse"] = {
   {
    id    = "cbsdId",
    reqd  = "Conditional",
    tdef  = "string",
        --[[ This is a CBRS-wide unique identifier for
           --  this CBSD. This parameter shall be
           --  included if and only if the responseCode
           --  indicates SUCCESS. The CBSD shall set
           --  its CBSD identity to the value received
           --  in this parameter. The string has a
           --  maximum length of 256 octets.]]
   },
   {
    id    = "measReportConfig",
    reqd  = "Optional",
    isray = true,
    tdef  = "array of string",
        --[[ SAS uses this parameter to configure CBSD
           --  measurement reporting. The measurement
           --  report requested by SAS shall be
           --  consistent with the CBSD measurement
           --  capabilities reported during the
           --  registration request. The CBSD shall
           --  report the measurement listed in this
           --  array. The permitted enumerations are
           --  specified in [n.21].]]
   },
   {
    id    = "response",
    reqd  = "Required",
    tdef  = "response",
        --[[ This parameter includes information on
           --  whether the corresponding CBSD request
           --  is approved or disapproved for a reason.
           --  See Table 14: Response Object
           --  Definition.]]
   },
},

["response"] = {
   {
    id    = "responseCode",
    reqd  = "Required",
    tdef  = "number",
        --[[ An integer to indicate the type of result.
           --  The value 0 means the corresponding CBSD
           --  request is successful.  This shall be
           --  one of the values in Table 39.]]
   },
   {
    id    = "responseMessage",
    reqd  = "Optional",
    tdef  = "string",
        --[[ A short description of the result.]]
   },
   {
    id    = "responseData",
    reqd  = "Optional",
    tdef  = "string",
        --[[ Additional data can be included to help
           --  the CBSD resolve failures.]]
   },
},

["spectrumInquiryRequest"] = {
   {
    id    = "cbsdId",
    reqd  = "Required",
    tdef  = "string",
        --[[ The CBSD shall set this parameter to the
           --  value of its CBSD identity.]]
   },
   {
    id    = "inquiredSpectrum",
    reqd  = "Required",
    isray = true,
    tdef  = "frequencyRange",
        --[[ This field describes the spectrum for
           --  which the CBSD seeks information on
           --  spectrum availability.]]
   },
   {
    id    = "measReport",
    reqd  = "Conditional",
    tdef  = "table",
        --[[ The CBSD uses this parameter to report
           --  measurements to the SAS. The format of
           --  the MeasReport object is provided in
           --  [n.21]. Refer to section 8 and the
           --  measurement capabilities in [n.21] for
           --  inclusion rules.]]
   },
},

["frequencyRange"] = {
   {
    id    = "lowFrequency",
    reqd  = "Required",
    tdef  = "number",
        --[[ The lowest frequency of the frequency
           --  range in Hz.]]
   },
   {
    id    = "highFrequency",
    reqd  = "Required",
    tdef  = "number",
        --[[ The highest frequency of the frequency
           --  range in Hz.]]
   },
},

["measReport"] = {
   {
    id    = "measReport",
    reqd  = "Required",
    tdef  = "table",
        --[[  "Defined"]]
   },
},

["spectrumInquiryResponse"] = {
   {
    id    = "cbsdId",
    reqd  = "Conditional",
    tdef  = "string",
        --[[ This parameter is included if and only if
           --  the cbsdId parameter in the
           --  SpectrumInquiryRequest object contains a
           --  valid CBSD identity. If included, the
           --  SAS shall set this parameter to the
           --  value of the cbsdId parameter in the
           --  corresponding SpectrumInquiryRequest
           --  object.]]
   },
   {
    id    = "availableChannel",
    reqd  = "Conditional",
    isray = true,
    tdef  = "availableChannel",
        --[[ This parameter is an array of zero or more
           --  data objects, AvailableChannel, which
           --  describes a channel that is available
           --  for the CBSD, see Table 21. Included: If
           --  and only if the Spectrum Inquiry is
           --  successful.]]
   },
   {
    id    = "response",
    reqd  = "Required",
    tdef  = "response",
        --[[ This parameter includes information on
           --  whether the corresponding CBSD request
           --  is approved or disapproved for a reason.
           --  See Table 14: Response Object
           --  Definition.]]
   },
},

["availableChannel"] = {
   {
    id    = "frequencyRange",
    reqd  = "Required",
    tdef  = "frequencyRange",
        --[[ This parameter is the frequency range of
           --  the available channel, see Table 17.]]
   },
   {
    id    = "channelType",
    reqd  = "Required",
    tdef  = "string",
        --[[ “PAL”: the frequency range is a PAL
           --  channel. “GAA”: the frequency range
           --  is for GAA use.]]
   },
   {
    id    = "ruleApplied",
    reqd  = "Required",
    tdef  = "string",
        --[[ The regulatory rule used to generate this
           --  response, e.g., “FCC_PART_96”.]]
   },
   {
    id    = "maxEirp",
    reqd  = "Optional",
    tdef  = "number",
        --[[ Maximum EIRP likely to be permitted for a
           --  Grant on this frequencyRange, given the
           --  CBSD registration parameters, including
           --  location, antenna orientation and
           --  antenna pattern. The maximum EIRP is in
           --  the units of dBm/MHz and is an integer
           --  with a value between -137 and +37
           --  (dBm/MHz) inclusive.]]
   },
},

["grantRequest"] = {
   {
    id    = "cbsdId",
    reqd  = "Required",
    tdef  = "string",
        --[[ The CBSD shall set this parameter to the
           --  value of its CBSD identity.]]
   },
   {
    id    = "operationParam",
    reqd  = "Required",
    tdef  = "operationParam",
        --[[ This data object includes operation
           --  parameters of the requested Grant.]]
   },
   {
    id    = "measReport",
    reqd  = "Conditional",
    tdef  = "table",
        --[[ The CBSD uses this parameter to report
           --  measurements to the SAS. The format of
           --  the MeasReport object is provided in
           --  [n.21]. Refer to section 8 and [n.21]
           --  for inclusion rules.]]
   },
},

["operationParam"] = {
   {
    id    = "maxEirp",
    reqd  = "Required",
    tdef  = "number",
        --[[  "Maximum EIRP permitted by the Grant. The
           --  maximum EIRP is in the units of dBm/MHz
           --  and is an integer with a value between
           --  -137 and +37 (dBm/MHz) inclusive. The
           --  value of maxEirp represents the average
           --  (RMS) EIRP that would be measured per
           --  the procedure defined in FCC
           --  §96.41(e)(3)."]]
   },
   {
    id    = "operationFrequencyRange",
    reqd  = "Required",
    tdef  = "frequencyRange",
        --[[  "This parameter is the frequency range of
           --  a contiguous segment"]]
   },
},

["grantResponse"] = {
   {
    id    = "cbsdId",
    reqd  = "Conditional",
    tdef  = "string",
        --[[ This parameter is included if and only if
           --  the cbsdId parameter in the GrantRequest
           --  object contains a valid CBSD identity.
           --  If included, the SAS shall set this
           --  parameter to the value of the cbsdId
           --  parameter in the corresponding
           --  GrantRequest object.]]
   },
   {
    id    = "grantId",
    reqd  = "Conditional",
    tdef  = "string",
        --[[ An ID provided by the SAS for this Grant.
           --  Included: If and only if the Grant
           --  request is approved by the SAS. The CBSD
           --  shall set the Grant identity for this
           --  Grant to the value received in this
           --  parameter.]]
   },
   {
    id    = "grantExpireTime",
    reqd  = "Conditional",
    tdef  = "string",
        --[[ The grantExpireTime indicates the time
           --  when the Grant associated with the
           --  grantId in this Heartbeat Response
           --  expires. This parameter is UTC time
           --  expressed in the format, YYYY-MM-
           --  DDThh:mm:ssZ as defined by [n.7]. This
           --  parameter shall be included if the
           --  responseCode parameter indicates SUCCESS
           --  or SUSPENDED_GRANT and the grantRenew
           --  parameter was included and set to True
           --  in the corresponding HeartbeatRequest
           --  object. This parameter may be included
           --  at other times by SAS choice.]]
   },
   {
    id    = "heartbeatInterval",
    reqd  = "Conditional",
    tdef  = "number",
        --[[ This parameter is a positive integer and
           --  indicates the maximum time interval in
           --  units of seconds between two consecutive
           --  heartbeat requests. This parameter is
           --  included when the SAS wants to change
           --  the heartbeat interval.]]
   },
   {
    id    = "measReportConfig",
    reqd  = "Optional",
    isray = true,
    tdef  = "array of string",
        --[[ The SAS uses this parameter to configure
           --  CBSD measurement reporting. The
           --  measurement report requested by the SAS
           --  shall be consistent with the CBSD
           --  measurement capabilities reported during
           --  the registration request. The CBSD shall
           --  report the measurements listed in this
           --  array. The permitted enumerations are
           --  specified in [n.21].]]
   },
   {
    id    = "operationParam",
    reqd  = "Optional",
    tdef  = "operationParam",
        --[[ If the Grant request is disapproved, using
           --  this object the SAS can optionally
           --  provide a new set of operation
           --  parameters to the CBSD for use in a new
           --  Grant request.]]
   },
   {
    id    = "channelType",
    reqd  = "Conditional",
    tdef  = "string",
        --[[ This parameter is included if and only if
           --  the responseCode parameter indicates
           --  SUCCESS, i.e., the Grant request was
           --  successful. “PAL”: the frequency
           --  range is a PAL channel. “GAA”: the
           --  frequency range is for GAA use.]]
   },
   {
    id    = "response",
    reqd  = "Required",
    tdef  = "response",
        --[[ This parameter includes information on
           --  whether the corresponding CBSD request
           --  is approved or disapproved for a reason.
           --  See Table 14.]]
   },
},

["heartbeatRequest"] = {
   {
    id    = "cbsdId",
    reqd  = "Required",
    tdef  = "string",
        --[[ The CBSD shall set this parameter to the
           --  value of its CBSD identity.]]
   },
   {
    id    = "grantId",
    reqd  = "Required",
    tdef  = "string",
        --[[ The CBSD shall set this parameter to the
           --  value of the Grant identity of this
           --  Grant.]]
   },
   {
    id    = "grantRenew",
    reqd  = "Optional",
    tdef  = "boolean",
        --[[ If set to True, the CBSD asks for renewal
           --  of the current Grant. SAS shall include
           --  a grantExpireTime parameter in the
           --  following HeartbeatResponse object.]]
   },
   {
    id    = "operationState",
    reqd  = "Required",
    tdef  = "string",
        --[[ This parameter contains the CBSD operation
           --  state (“AUTHORIZED” or
           --  “GRANTED”).]]
   },
   {
    id    = "measReport",
    reqd  = "Conditional",
    tdef  = "table",
        --[[ The CBSD uses this parameter to report
           --  measurements to the SAS. The format of
           --  the MeasReport object is provided in
           --  [n.21]. Refer to section 8 and [n.21]
           --  for inclusion rules.]]
   },
},

["heartbeatResponse"] = {
   {
    id    = "cbsdId",
    reqd  = "Conditional",
    tdef  = "string",
        --[[ This parameter is included if and only if
           --  the cbsdId parameter in the
           --  HeartbeatRequest object contains a valid
           --  CBSD identity. If included, the SAS
           --  shall set this parameter to the value of
           --  the cbsdId parameter in the
           --  corresponding HeartbeatRequest object.]]
   },
   {
    id    = "grantId",
    reqd  = "Conditional",
    tdef  = "string",
        --[[ This parameter is included if and only if
           --  the grantId parameter in the
           --  HeartbeatRequest object contains a valid
           --  Grant identity. If included, the SAS
           --  shall set this parameter to the value of
           --  the grantId parameter in the
           --  corresponding HeartbeatRequest object.]]
   },
   {
    id    = "transmitExpireTime",
    reqd  = "Required",
    tdef  = "string",
        --[[ The CBSD shall cease transmission using
           --  the SAS authorized radio resource within
           --  60 seconds after the value of the
           --  transmitExpireTime parameter expires, in
           --  accordance with part 96.39(c)(2) (ref.
           --  [n.8]). The transmitExpireTime is UTC
           --  time expressed in the format, YYYY-
           --  MM-DDThh:mm:ssZ as defined by [n.7]. The
           --  transmitExpireTime value shall be no
           --  later than the grantExpireTime.]]
   },
   {
    id    = "grantExpireTime",
    reqd  = "Conditional",
    tdef  = "string",
        --[[ Required if the responseCode parameter
           --  indicates SUCCESS or SUSPENDED_GRANT and
           --  the grantRenew parameter was included
           --  and set to True in the corresponding
           --  HeartbeatRequest object. This parameter
           --  may be included at other times by SAS
           --  choice.]]
   },
   {
    id    = "heartbeatInterval",
    reqd  = "Optional",
    tdef  = "number",
        --[[ This parameter is a positive integer and
           --  indicates the maximum time interval in
           --  units of seconds between two consecutive
           --  heartbeat requests. This parameter is
           --  included when the SAS wants to change
           --  the heartbeat interval.]]
   },
   {
    id    = "operationParam",
    reqd  = "Optional",
    tdef  = "operationParam",
        --[[ If heartbeat request is disapproved or the
           --  SAS intends to change the CBSD operation
           --  parameters, the SAS can provide a new
           --  set of operation parameters to the CBSD
           --  using this object.]]
   },
   {
    id    = "measReportConfig",
    reqd  = "Optional",
    isray = true,
    tdef  = "array of string",
        --[[ The SAS uses this parameter to configure
           --  CBSD measurement reporting. The
           --  measurement report requested by the SAS
           --  shall be consistent with the CBSD
           --  measurement capabilities reported during
           --  the registration request. The CBSD shall
           --  report the measurement listed in this
           --  array. The permitted enumerations are
           --  specified in [n.21].]]
   },
   {
    id    = "response",
    reqd  = "Required",
    tdef  = "response",
        --[[ This parameter includes information on
           --  whether the corresponding CBSD request
           --  is approved or disapproved for a reason.
           --  See Table 14.]]
   },
},

["relinquishmentRequest"] = {
   {
    id    = "cbsdId",
    reqd  = "Required",
    tdef  = "string",
        --[[ The CBSD shall set this parameter to the
           --  value of its CBSD identity.]]
   },
   {
    id    = "grantId",
    reqd  = "Required",
    tdef  = "string",
        --[[ The CBSD shall set this parameter to the
           --  value of the Grant identity of this
           --  Grant.]]
   },
},

["relinquishmentResponse"] = {
   {
    id    = "cbsdId",
    reqd  = "Conditional",
    tdef  = "string",
        --[[ This parameter is included if and only if
           --  the cbsdId parameter in the
           --  RelinquishmentRequest object contains a
           --  valid CBSD identity. If included, the
           --  SAS shall set this parameter to the
           --  value of the cbsdId parameter in the
           --  corresponding RelinquishmentRequest
           --  object.]]
   },
   {
    id    = "grantId",
    reqd  = "Conditional",
    tdef  = "string",
        --[[ This parameter is included if and only if
           --  the grantId parameter in the
           --  RelinquishmentRequest object contains a
           --  valid Grant identity. If included, the
           --  SAS shall set this parameter to the
           --  value of the grantId parameter in the
           --  corresponding RelinquishmentRequest
           --  object.]]
   },
   {
    id    = "response",
    reqd  = "Required",
    tdef  = "response",
        --[[ This parameter includes information on
           --  whether the corresponding CBSD request
           --  is approved or disapproved for a reason.
           --  See Table 14: Response Object
           --  Definition.]]
   },
},

["deregistrationRequest"] = {
   {
    id    = "cbsdId",
    reqd  = "Required",
    tdef  = "string",
        --[[ The CBSD shall set this parameter to the
           --  value of its CBSD identity.]]
   },
},

["deregistrationResponse"] = {
   {
    id    = "cbsdId",
    reqd  = "Conditional",
    tdef  = "string",
        --[[ This parameter is included if and only if
           --  the cbsdId parameter in the
           --  DeregistrationRequest object contains a
           --  valid CBSD identity. If included, the
           --  SAS shall set this parameter to the
           --  value of the cbsdId parameter in the
           --  corresponding DeregistrationRequest
           --  object.]]
   },
   {
    id    = "response",
    reqd  = "Required",
    tdef  = "response",
        --[[ This parameter includes information on
           --  whether the corresponding CBSD request
           --  is approved or disapproved for a reason.
           --  See Table 14: Response Object
           --  Definition.]]
   },
},

}
return { tabdef_msg, tabdef_obj }
