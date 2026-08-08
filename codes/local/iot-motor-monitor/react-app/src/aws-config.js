// Fill these in from your SAM stack outputs (`sam deploy` prints them,
// or: aws cloudformation describe-stacks --stack-name <name> --query Stacks[0].Outputs)

export const AWS_CONFIG = {
  region: "REGION",
  iotEndpoint: "xxxxxxxxxxxxx-ats.iot.REGION.amazonaws.com",
  userPoolId: "REGION_xxxxxxxxx",
  userPoolClientId: "xxxxxxxxxxxxxxxxxxxxxxxxxx",
  identityPoolId: "REGION:xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx",
  apiEndpoint: "https://xxxxxxxxxx.execute-api.REGION.amazonaws.com/prod",
};
